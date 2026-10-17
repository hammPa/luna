#include "auth/totp.hpp"

#include <cstdlib>
#include <iostream>

static std::string code_for(long long step) { return std::to_string(100000 + step % 900000); }
static TotpCodeFn fake() {
    return [](std::time_t t) { return code_for(static_cast<long long>(t) / 30); };
}
static const char* name(TotpResult r) {
    switch (r) {
        case TotpResult::Ok: return "Ok";
        case TotpResult::Wrong: return "Wrong";
        case TotpResult::Reused: return "Reused";
        case TotpResult::Locked: return "Locked";
        case TotpResult::Error: return "Error";
    }
    return "?";
}
static void expect(const char* label, TotpResult got, TotpResult want) {
    std::cout << (got == want ? "OK    " : "GAGAL ") << label << " -> " << name(got)
              << " (harus " << name(want) << ")\n";
}

int main() {
    const std::time_t now = 1000000020;  // kelipatan 30
    const long long s = now / 30;

    { TotpVerifier v(fake());
      expect("A1 kode sekarang", v.verify(code_for(s), now), TotpResult::Ok);
      expect("A2 dipakai ulang", v.verify(code_for(s), now), TotpResult::Reused); }

    { TotpVerifier v(fake());
      expect("B1 langkah depan", v.verify(code_for(s + 1), now), TotpResult::Ok);
      expect("B2 langkah lama sesudahnya", v.verify(code_for(s), now), TotpResult::Reused); }

    { TotpVerifier v(fake());
      expect("C langkah belakang (-1)", v.verify(code_for(s - 1), now), TotpResult::Ok); }

    { TotpVerifier v(fake());
      expect("D kedaluwarsa (-2)", v.verify(code_for(s - 2), now), TotpResult::Wrong); }

    { TotpVerifier v(fake());
      expect("E1 bukan angka", v.verify("abc", now), TotpResult::Wrong);
      expect("E2 5 digit", v.verify("12345", now), TotpResult::Wrong);
      expect("E3 salah", v.verify("000000", now), TotpResult::Wrong);
      expect("E4 benar tapi terkunci", v.verify(code_for(s), now), TotpResult::Locked); }

    { TotpVerifier v(fake());
      v.verify("000000", now); v.verify("000000", now);
      expect("G1 sukses mereset hitungan", v.verify(code_for(s), now), TotpResult::Ok);
      v.verify("000000", now + 30); v.verify("000000", now + 30);
      expect("G2 belum terkunci", v.verify(code_for(s + 1), now + 30), TotpResult::Ok); }

    { TotpVerifier v([](std::time_t) -> std::string { throw std::runtime_error("x"); });
      expect("H error sistem", v.verify("123456", now), TotpResult::Error); }

    if (const char* sec = std::getenv("LUNA_TOTP_SECRET")) {
        TotpVerifier v(make_oathtool_generator(sec));
        std::cout << "\nKetik kode dari app authenticator: ";
        std::string c;
        std::getline(std::cin, c);
        std::cout << "oathtool asli -> " << name(v.verify(c)) << " (harus Ok)\n";
        std::cout << "kode yang sama lagi -> " << name(v.verify(c)) << " (harus Reused)\n";
    }
}