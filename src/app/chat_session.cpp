#include "app/chat_session.hpp"

#include <iostream>
#include <utility>

ChatSession::ChatSession(const Config& cfg, LLMBackend& backend, Memory& memory)
    : cfg_(cfg), backend_(backend), memory_(memory) {
    history_.push_back({"system", cfg_.system_prompt, ""});
}

void ChatSession::load() {
    for (auto& m : memory_.load_recent(cfg_.history_limit)) {
        history_.push_back(std::move(m));
    }
    trim_history();
}

void ChatSession::trim_history() {
    // Potong yang tertua (indeks 1); system prompt di indeks 0 dipertahankan.
    while (history_.size() - 1 > cfg_.history_limit) {
        history_.erase(history_.begin() + 1);
    }
    // Pesan tertua setelah system harus berperan "user".
    while (history_.size() > 1 && history_[1].role != "user") {
        history_.erase(history_.begin() + 1);
    }
}

std::string ChatSession::build_system_prompt() {
    std::vector<std::pair<std::string, std::string>> facts;
    try {
        facts = memory_.all_facts();
    } catch (const std::exception& e) {
        std::cerr << "Peringatan: gagal membaca fakta: " << e.what() << '\n';
    }
    if (facts.empty()) return cfg_.system_prompt;

    std::string out = cfg_.system_prompt + "\n\nFakta tentang pengguna:\n";
    for (const auto& [k, v] : facts) {
        out += "- " + k + ": " + v + "\n";
    }
    return out;
}

std::string ChatSession::send(const std::string& user_text) {
    // Fakta bisa berubah di tengah sesi, jadi system prompt dibangun ulang.
    history_[0].content = build_system_prompt();

    history_.push_back({"user", user_text, ""});
    trim_history();

    LLMResponse r;
    try {
        r = backend_.chat(history_, nlohmann::json::array());
    } catch (...) {
        history_.pop_back();  // rollback pesan user
        throw;                // lempar ulang ke pemanggil
    }

    history_.push_back({"assistant", r.text, ""});

    // Gagal simpan DB tidak mematikan chat.
    try {
        memory_.add_message({"user", user_text, ""});
        memory_.add_message({"assistant", r.text, ""});
    } catch (const std::exception& e) {
        std::cerr << "Peringatan: gagal menyimpan ke database: " << e.what() << '\n';
    }

    return r.text;
}