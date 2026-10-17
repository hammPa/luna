#include "auth/totp.hpp"

#include "utils/process_utils.hpp"
#include "utils/string_utils.hpp"

#include <cctype>
#include <stdexcept>
#include <utility>

namespace {

bool is_six_digits(const std::string& s) {
    if (s.size() != 6) return false;
    for (unsigned char c : s) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

// Bandingkan tanpa berhenti di karakter pertama yang berbeda.
bool const_eq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

}  // namespace

TotpCodeFn make_oathtool_generator(std::string secret) {
    return [secret = std::move(secret)](std::time_t t) -> std::string {
        std::tm tm_buf{};
        gmtime_r(&t, &tm_buf);
        char ts[40];
        if (std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S UTC", &tm_buf) == 0) {
            throw std::runtime_error("Gagal memformat waktu");
        }
        CaptureResult r = run_capture({"oathtool", "--totp", "-b", "--now", ts, "-"},
                                      secret + "\n");
        if (r.exit_code != 0) {
            throw std::runtime_error("oathtool gagal (kode " + std::to_string(r.exit_code) + ")");
        }
        std::string code = trim(r.out);
        if (!is_six_digits(code)) throw std::runtime_error("Keluaran oathtool tidak valid");
        return code;
    };
}

TotpVerifier::TotpVerifier(TotpCodeFn gen, int max_failures, int lock_minutes)
    : gen_(std::move(gen)), max_failures_(max_failures), lock_for_(lock_minutes) {}

TotpResult TotpVerifier::verify(const std::string& code, std::time_t now) {
    using Clock = std::chrono::steady_clock;
    if (Clock::now() < locked_until_) return TotpResult::Locked;

    auto fail = [&](TotpResult why) {
        if (++failures_ >= max_failures_) {
            locked_until_ = Clock::now() + lock_for_;
            failures_ = 0;
        }
        return why;
    };

    if (!is_six_digits(code)) return fail(TotpResult::Wrong);

    const long long base = static_cast<long long>(now) / kStepSec;
    try {
        for (int off = -1; off <= 1; ++off) {
            const long long step = base + off;
            if (!const_eq(gen_(static_cast<std::time_t>(step * kStepSec)), code)) continue;

            if (step <= last_step_) return fail(TotpResult::Reused);  // kode lama dipakai ulang
            last_step_ = step;
            failures_ = 0;
            return TotpResult::Ok;
        }
    } catch (const std::exception&) {
        return TotpResult::Error;  // kegagalan sistem, tidak dihitung sebagai salah
    }
    return fail(TotpResult::Wrong);
}