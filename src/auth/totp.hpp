#pragma once

#include <chrono>
#include <ctime>
#include <functional>
#include <string>

enum class TotpResult { Ok, Wrong, Reused, Locked, Error };

// Menghasilkan kode 6 digit untuk waktu t (detik epoch UTC). Lempar exception kalau gagal.
using TotpCodeFn = std::function<std::string(std::time_t)>;

// Generator memakai oathtool. Secret dikirim lewat stdin, tidak muncul di ps.
TotpCodeFn make_oathtool_generator(std::string secret);

class TotpVerifier {
public:
    explicit TotpVerifier(TotpCodeFn gen, int max_failures = 3, int lock_minutes = 10);

    TotpResult verify(const std::string& code, std::time_t now = std::time(nullptr));

private:
    static constexpr int kStepSec = 30;

    TotpCodeFn gen_;
    int max_failures_;
    std::chrono::minutes lock_for_;
    int failures_ = 0;
    long long last_step_ = -1;
    std::chrono::steady_clock::time_point locked_until_{};
};