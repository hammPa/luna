#pragma once

#include "telegram/telegram_client.hpp"
#include "tools/prompter.hpp"

#include <chrono>
#include <deque>

// Menanyakan izin lewat Telegram. Sebelum bind() dan set_context(),
// semua permintaan ditolak (fail closed).
class TelegramPrompter : public Prompter {
public:
    // Dipanggil run_telegram setelah client dibuat. Pointer harus hidup selama dipakai.
    void bind(TelegramClient* tg, long long* offset);

    // Siapa yang sedang dilayani. Panggil sebelum session.send(), clear sesudahnya.
    void set_context(long long chat_id, long long user_id);
    void clear_context();

    Decision ask(const std::string& question, bool can_always, Risk risk) override;

private:
    bool rate_limited();

    static constexpr int kTimeoutSec = 60;
    static constexpr std::size_t kMaxApprovals = 5;
    static constexpr int kWindowMin = 10;

    TelegramClient* tg_ = nullptr;
    long long* offset_ = nullptr;
    long long chat_id_ = 0;
    long long user_id_ = 0;
    std::deque<std::chrono::steady_clock::time_point> history_;
};