#include "app/chat_session.hpp"

#include <iostream>
#include <utility>

ChatSession::ChatSession(const Config& cfg, LLMBackend& backend, Memory& memory,
                         ToolRegistry& tools)
    : cfg_(cfg), backend_(backend), memory_(memory), tools_(tools) {
    history_.push_back(make_msg("system", cfg_.system_prompt));
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
    std::string out = cfg_.system_prompt;

    if (!tools_.empty()) {
        out += "\n\nTool yang tersedia:\n" + tools_.describe_all();
    }

    std::vector<std::pair<std::string, std::string>> facts;
    try {
        facts = memory_.all_facts();
    } catch (const std::exception& e) {
        std::cerr << "Peringatan: gagal membaca fakta: " << e.what() << '\n';
    }
    if (!facts.empty()) {
        out += "\n\nFakta tentang pengguna:\n";
        for (const auto& [k, v] : facts) {
            out += "- " + k + ": " + v + "\n";
        }
    }
    return out;
}

void ChatSession::save_turn(const std::string& user, const std::string& reply) {
    // Hanya user dan jawaban akhir yang disimpan. Pesan perantara tool
    // hidup di RAM selama giliran ini. Gagal simpan tidak mematikan chat.
    try {
        memory_.add_message(make_msg("user", user));
        memory_.add_message(make_msg("assistant", reply));
    } catch (const std::exception& e) {
        std::cerr << "Peringatan: gagal menyimpan ke database: " << e.what() << '\n';
    }
}

std::string ChatSession::send(const std::string& user_text) {
    history_[0].content = build_system_prompt();

    // Trim hanya di awal giliran. Jangan di tengah loop tool, karena
    // pesan "tool" tanpa assistant pemintanya akan ditolak API.
    trim_history();

    const std::size_t start = history_.size();  // titik rollback
    history_.push_back(make_msg("user", user_text));

    try {
        for (int round = 0; round < kMaxToolRounds; ++round) {
            LLMResponse r = backend_.chat(history_, tools_.schema());

            if (!r.wants_toolcall()) {
                history_.push_back(make_msg("assistant", r.text));
                save_turn(user_text, r.text);
                return r.text;
            }

            // Assistant meminta tool: simpan permintaannya di history.
            Message a = make_msg("assistant", r.text);
            a.tool_calls = r.toolcalls;
            history_.push_back(std::move(a));

            // Jalankan tiap tool (gerbang permission ada di registry).
            for (const auto& c : r.toolcalls) {
                Message t = make_msg("tool", tools_.run(c.name, c.arguments));
                t.tool_call_id = c.id;
                t.name = c.name;
                history_.push_back(std::move(t));
            }
        }
    } catch (...) {
        history_.resize(start);  // buang semua pesan giliran ini
        throw;
    }

    // Putaran habis tanpa jawaban akhir.
    history_.resize(start);
    throw std::runtime_error("Terlalu banyak langkah tool (maks " +
                             std::to_string(kMaxToolRounds) + "), dibatalkan.");
}

void ChatSession::clear() {
    memory_.clear_messages();   // DB dulu: kalau gagal, RAM tidak berubah
    history_.resize(1);         // sisakan system prompt di indeks 0
}