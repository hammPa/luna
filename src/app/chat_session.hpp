#pragma once

#include "llm/llm_backend.hpp"
#include "memory/memory.hpp"
#include "utils/config.hpp"

#include <cstddef>
#include <string>
#include <vector>

// Satu percakapan: memegang history, menyambungkan LLMBackend dan Memory.
// Menyimpan referensi, jadi objek yang dirujuk harus hidup lebih lama.
class ChatSession {
public:
    ChatSession(const Config& cfg, LLMBackend& backend, Memory& memory);

    ChatSession(const ChatSession&) = delete;
    ChatSession& operator=(const ChatSession&) = delete;

    // Muat riwayat dari database. Panggil sekali saat start.
    void load();

    // Satu giliran chat. Mengembalikan teks balasan.
    // Jika chat gagal: lempar exception, history sudah dikembalikan seperti semula.
    std::string send(const std::string& user_text);

    std::size_t history_size() const { return history_.size() - 1; }  // tanpa system

private:
    void trim_history();
    std::string build_system_prompt();

    const Config& cfg_;
    LLMBackend& backend_;
    Memory& memory_;
    std::vector<Message> history_;  // indeks 0 selalu system prompt
};