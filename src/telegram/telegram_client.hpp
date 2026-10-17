#pragma once

#include "utils/http.hpp"
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

struct TelegramUpdate {
    long long update_id = 0;
    long long user_id = 0;
    long long chat_id = 0;
    long long message_id = 0;
    std::string text;
};

class TelegramClient {
public:
    explicit TelegramClient(std::string token);

    // Long polling. Lempar runtime_error kalau jaringan/API gagal.
    // `next_offset` diperbarui ke update_id terakhir + 1.
    std::vector<TelegramUpdate> get_updates(long long& next_offset, int poll_timeout_sec = 25);

    // Pesan panjang dipecah otomatis.
    void send_message(long long chat_id, const std::string& text);

    // Tampilkan "sedang mengetik..." (hilang sendiri setelah ~5 detik).
    void send_chat_action(long long chat_id, const std::string& action = "typing");

    // Buang semua update yang menumpuk, supaya pesan lama tidak dieksekusi.
    void skip_backlog(long long& next_offset);

    // Hapus pesan. Gagal (mis. sudah terlalu lama) tidak fatal.
    void delete_message(long long chat_id, long long message_id);
private:
    nlohmann::json call(const std::string& method, const nlohmann::json& body, long http_timeout);

    HttpClient http_;
    std::string base_;  // https://api.telegram.org/bot<TOKEN>/
};

// Dipisah agar bisa dites sendiri.
std::vector<std::string> split_message(const std::string& text, std::size_t max = 4000);