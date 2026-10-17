#include "telegram/telegram_prompter.hpp"

#include "utils/string_utils.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

using Clock = std::chrono::steady_clock;

void TelegramPrompter::bind(TelegramClient* tg, long long* offset) {
    tg_ = tg;
    offset_ = offset;
}

void TelegramPrompter::set_context(long long chat_id, long long user_id) {
    chat_id_ = chat_id;
    user_id_ = user_id;
}

void TelegramPrompter::clear_context() {
    chat_id_ = 0;
    user_id_ = 0;
}

// Maksimal kMaxApprovals permintaan per kWindowMin menit.
bool TelegramPrompter::rate_limited() {
    const auto now = Clock::now();
    const auto window = std::chrono::minutes(kWindowMin);
    while (!history_.empty() && now - history_.front() > window) history_.pop_front();
    if (history_.size() >= kMaxApprovals) return true;
    history_.push_back(now);
    return false;
}

Decision TelegramPrompter::ask(const std::string& question, bool /*can_always*/, Risk risk) {
    if (tg_ == nullptr || offset_ == nullptr || chat_id_ == 0) return Decision::Deny;

    try {
        if (risk == Risk::Destructive || risk == Risk::Privileged) {
            if (verifier_ == nullptr) {
                tg_->send_message(chat_id_, question +
                    "\n\nDITOLAK: aksi ini butuh kode authenticator, tapi LUNA_TOTP_SECRET belum di-set.");
                return Decision::Deny;
            }
            if (rate_limited()) {
                tg_->send_message(chat_id_,
                    "Terlalu banyak permintaan izin dalam waktu singkat. Ditolak otomatis.");
                return Decision::Deny;
            }
            return ask_totp(question);
        }

        if (rate_limited()) {
            tg_->send_message(chat_id_,
                "Terlalu banyak permintaan izin dalam waktu singkat. Ditolak otomatis.");
            return Decision::Deny;
        }

        // Buang pesan yang sudah menumpuk supaya "y" lama tidak dianggap jawaban.
        tg_->skip_backlog(*offset_);

        tg_->send_message(chat_id_, question +
            "\n\nBalas y untuk izinkan atau n untuk tolak (" +
            std::to_string(kTimeoutSec) + " detik).");

        const auto deadline = Clock::now() + std::chrono::seconds(kTimeoutSec);
        while (Clock::now() < deadline) {
            for (const auto& u : tg_->get_updates(*offset_, 5)) {
                // Hanya orang dan chat yang sedang dilayani.
                if (u.user_id != user_id_ || u.chat_id != chat_id_) continue;

                std::string a = trim(u.text);
                std::transform(a.begin(), a.end(), a.begin(),
                               [](unsigned char c) { return std::tolower(c); });

                if (a == "y" || a == "ya" || a == "yes") return Decision::AllowOnce;
                if (a == "n" || a == "no" || a == "tidak") return Decision::Deny;

                tg_->send_message(chat_id_, "Jawab y atau n saja.");
            }
        }
        tg_->send_message(chat_id_, "Waktu habis, aksi ditolak.");
    } catch (const std::exception& e) {
        std::cerr << "[telegram] prompter error: " << e.what() << '\n';
    }
    return Decision::Deny;  // fail closed
}



Decision TelegramPrompter::ask_totp(const std::string& question) {
    try {
        tg_->skip_backlog(*offset_);

        tg_->send_message(chat_id_, question +
            "\n\nAKSI BERBAHAYA. Balas dengan kode 6 digit dari app authenticator, "
            "atau n untuk menolak (" + std::to_string(kTimeoutSec) + " detik).");

        int attempts = 0;
        const auto deadline = Clock::now() + std::chrono::seconds(kTimeoutSec);
        while (Clock::now() < deadline) {
            for (const auto& u : tg_->get_updates(*offset_, 5)) {
                if (u.user_id != user_id_ || u.chat_id != chat_id_) continue;

                std::string a = trim(u.text);
                std::string lower = a;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (lower == "n" || lower == "no" || lower == "tidak") return Decision::Deny;

                // Apa pun yang dikirim di sini dianggap percobaan kode: hapus dari chat.
                TotpResult r = verifier_->verify(a);
                try { tg_->delete_message(u.chat_id, u.message_id); } catch (const std::exception&) {}

                switch (r) {
                    case TotpResult::Ok:
                        tg_->send_message(chat_id_, "Kode benar, aksi dijalankan.");
                        return Decision::AllowOnce;
                    case TotpResult::Locked:
                        tg_->send_message(chat_id_, "Terkunci sementara karena terlalu banyak salah. Ditolak.");
                        return Decision::Deny;
                    case TotpResult::Error:
                        tg_->send_message(chat_id_, "Verifikasi gagal (kesalahan sistem). Ditolak.");
                        return Decision::Deny;
                    case TotpResult::Reused:
                        tg_->send_message(chat_id_, "Kode itu sudah dipakai. Tunggu kode berikutnya lalu kirim lagi.");
                        break;
                    case TotpResult::Wrong:
                        tg_->send_message(chat_id_, "Kode salah.");
                        break;
                }
                if (++attempts >= kMaxAttempts) {
                    tg_->send_message(chat_id_, "Terlalu banyak percobaan. Ditolak.");
                    return Decision::Deny;
                }
            }
        }
        tg_->send_message(chat_id_, "Waktu habis, aksi ditolak.");
    } catch (const std::exception& e) {
        std::cerr << "[telegram] totp prompter error: " << e.what() << '\n';
    }
    return Decision::Deny;  // fail closed
}