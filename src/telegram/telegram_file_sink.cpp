#include "telegram/telegram_file_sink.hpp"

#include <stdexcept>

std::string TelegramFileSink::deliver(const std::filesystem::path& file, const std::string& caption) {
    if (tg_ == nullptr || chat_id_ == 0) {
        throw std::runtime_error("Kanal Telegram belum siap, file tidak dikirim.");
    }

    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);

    tg_->send_document(chat_id_, file, caption);  // melempar runtime_error jika gagal

    return file.filename().string() + (ec ? std::string() : " (" + human_size(size) + ")");
}
