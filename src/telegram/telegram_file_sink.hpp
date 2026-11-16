#pragma once

#include "telegram/telegram_client.hpp"
#include "tools/file_sink.hpp"

// Mengirim file ke chat Telegram yang sedang dilayani. Sebelum bind() dan
// set_context(), pengiriman ditolak (fail closed), sama seperti TelegramPrompter.
class TelegramFileSink : public FileSink {
public:
    // Dipanggil run_telegram setelah client dibuat. Pointer harus hidup selama dipakai.
    void bind(TelegramClient* tg) { tg_ = tg; }

    // Chat yang sedang dilayani. Panggil sebelum session.send(), clear sesudahnya.
    void set_context(long long chat_id) { chat_id_ = chat_id; }
    void clear_context() { chat_id_ = 0; }

    bool uploads() const override { return true; }

    std::string deliver(const std::filesystem::path& file, const std::string& caption) override;

private:
    TelegramClient* tg_ = nullptr;
    long long chat_id_ = 0;
};
