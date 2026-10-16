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
        // Aksi berbahaya belum boleh disetujui dari Telegram (menunggu TOTP di 5b/5c).
        if (risk == Risk::Destructive || risk == Risk::Privileged) {
            tg_->send_message(chat_id_, question +
                "\n\nDITOLAK: aksi ini belum bisa disetujui dari Telegram.");
            return Decision::Deny;
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