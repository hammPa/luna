#include "app/chat_session.hpp"
#include "utils/log.hpp"
#include "utils/ctx_format.hpp"

#include <chrono>
#include <iostream>
#include <utility>

namespace {

// Perkiraan kasar ukuran yang dikirim: isi pesan + argumen tool + schema.
int estimate_context(const std::vector<Message>& history, const nlohmann::json& schema) {
    std::size_t bytes = schema.dump(-1, ' ', false,
                                    nlohmann::json::error_handler_t::replace).size();
    for (const auto& m : history) {
        bytes += m.content.size() + m.name.size();
        for (const auto& c : m.tool_calls) {
            bytes += c.name.size() + c.arguments.dump(
                -1, ' ', false, nlohmann::json::error_handler_t::replace).size();
        }
    }
    return ctx_fmt::estimate_tokens(bytes);
}

}  // namespace

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
            const auto t0 = std::chrono::steady_clock::now();

            const auto schema = tools_.schema();
            LLMResponse r = backend_.chat(history_, schema);
            
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - t0).count();
            logger::info("llm", "chat round=" + std::to_string(round) +
                                " ms=" + std::to_string(ms) +
                                " tool_calls=" + std::to_string(r.toolcalls.size()));

            // Catat ukuran konteks yang dipakai. Jika provider tidak mengirim usage,
            if (r.prompt_tokens > 0) {
                ctx_used_ = r.prompt_tokens;
                ctx_estimated_ = false;
            } else {
                ctx_used_ = estimate_context(history_, schema);
                ctx_estimated_ = true;
            }
            
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
    } catch (const std::exception& e) {
        logger::error("llm", e.what());
        history_.resize(start);
        throw;
    } catch (...) {
        history_.resize(start);
        throw;
    }

    // Putaran habis tanpa jawaban akhir.
    history_.resize(start);
    logger::warn("llm", "putaran tool habis tanpa jawaban akhir");
    throw std::runtime_error("Terlalu banyak langkah tool (maks " +
                             std::to_string(kMaxToolRounds) + "), dibatalkan.");
}

void ChatSession::clear() {
    memory_.clear_messages();   // DB dulu: kalau gagal, RAM tidak berubah
    history_.resize(1);         // sisakan system prompt di indeks 0
    ctx_used_ = -1;             // konteks sudah kosong; angka lama tidak berlaku
}