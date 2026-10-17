#include "telegram/telegram_client.hpp"

#include <stdexcept>

using json = nlohmann::json;

TelegramClient::TelegramClient(std::string token)
    : base_("https://api.telegram.org/bot" + std::move(token) + "/") {}

json TelegramClient::call(const std::string& method, const json& body, long http_timeout) {
    HttpResponse res = http_.post(base_ + method,
                                  {"Content-Type: application/json"},
                                  body.dump(), http_timeout);
    json j;
    try {
        j = json::parse(res.body);
    } catch (const json::exception&) {
        throw std::runtime_error("Telegram: balasan bukan JSON (HTTP " +
                                 std::to_string(res.statusCode) + ")");
    }
    if (!j.value("ok", false)) {
        // Jangan sertakan URL (berisi token).
        throw std::runtime_error("Telegram API error " + std::to_string(res.statusCode) + ": " +
                                 j.value("description", std::string("tanpa deskripsi")));
    }
    return j;
}

std::vector<TelegramUpdate> TelegramClient::get_updates(long long& next_offset,
                                                        int poll_timeout_sec) {
    json body = {{"offset", next_offset},
                 {"timeout", poll_timeout_sec},
                 {"allowed_updates", json::array({"message"})}};
    json j = call("getUpdates", body, poll_timeout_sec + 10);

    std::vector<TelegramUpdate> out;
    if (!j.contains("result") || !j["result"].is_array()) return out;

    for (const auto& u : j["result"]) {
        if (!u.is_object() || !u.contains("update_id")) continue;
        long long id = u["update_id"].get<long long>();
        if (id >= next_offset) next_offset = id + 1;  // selalu maju, walau pesan dilewati

        if (!u.contains("message") || !u["message"].is_object()) continue;
        const json& m = u["message"];
        if (!m.contains("text") || !m["text"].is_string()) continue;
        if (!m.contains("from") || !m.contains("chat")) continue;

        TelegramUpdate t;
        t.update_id = id;
        t.user_id = m["from"].value("id", 0LL);
        t.chat_id = m["chat"].value("id", 0LL);
        t.message_id = m.value("message_id", 0LL);
        t.text = m["text"].get<std::string>();
        out.push_back(std::move(t));
    }
    return out;
}

void TelegramClient::send_message(long long chat_id, const std::string& text) {
    for (const auto& part : split_message(text)) {
        call("sendMessage", {{"chat_id", chat_id}, {"text", part}}, 15);
    }
}

std::vector<std::string> split_message(const std::string& text, std::size_t max) {
    std::vector<std::string> parts;
    if (text.empty()) return {"(kosong)"};
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t len = std::min(max, text.size() - pos);
        if (pos + len < text.size()) {
            // Cari batas baris terakhir dalam potongan.
            std::size_t nl = text.rfind('\n', pos + len);
            if (nl != std::string::npos && nl > pos) len = nl - pos;
            // Jangan memotong di tengah karakter UTF-8.
            while (len > 1 && (static_cast<unsigned char>(text[pos + len]) & 0xC0) == 0x80) --len;
        }
        parts.push_back(text.substr(pos, len));
        pos += len;
        if (pos < text.size() && text[pos] == '\n') ++pos;
    }
    return parts;
}

void TelegramClient::send_chat_action(long long chat_id, const std::string& action) {
    call("sendChatAction", {{"chat_id", chat_id}, {"action", action}}, 10);
}

void TelegramClient::skip_backlog(long long& next_offset) {
    // offset -1 = hanya update terakhir; cukup untuk tahu ID paling baru.
    json j = call("getUpdates", {{"offset", -1}, {"timeout", 0}}, 15);
    if (!j.contains("result") || !j["result"].is_array()) return;
    for (const auto& u : j["result"]) {
        if (u.is_object() && u.contains("update_id")) {
            next_offset = std::max(next_offset, u["update_id"].get<long long>() + 1);
        }
    }
}


void TelegramClient::delete_message(long long chat_id, long long message_id) {
    call("deleteMessage", {{"chat_id", chat_id}, {"message_id", message_id}}, 10);
}