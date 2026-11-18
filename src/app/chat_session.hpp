#pragma once

#include "llm/llm_backend.hpp"
#include "memory/memory.hpp"
#include "tools/tool_registry.hpp"
#include "utils/config.hpp"

#include <cstddef>
#include <string>
#include <vector>
#include <atomic>

// Satu percakapan: memegang history, menyambungkan LLMBackend dan Memory.
// Menyimpan referensi, jadi objek yang dirujuk harus hidup lebih lama.
class ChatSession {
public:
    ChatSession(const Config& cfg, LLMBackend& backend, Memory& memory,
                ToolRegistry& tools);

    ChatSession(const ChatSession&) = delete;
    ChatSession& operator=(const ChatSession&) = delete;

    // Muat riwayat dari database. Panggil sekali saat start.
    void load();

    // Satu giliran chat. Mengembalikan teks balasan.
    // Jika chat gagal: lempar exception, history sudah dikembalikan seperti semula.
    std::string send(const std::string& user_text);

    std::size_t history_size() const { return history_.size() - 1; }  // tanpa system

    // Ukuran konteks yang terpakai pada panggilan LLM terakhir (token).
    // -1 = belum ada. estimated: angka perkiraan, bukan dari provider.
    int  ctx_used() const { return ctx_used_; }
    bool ctx_estimated() const { return ctx_estimated_; }
    int  ctx_max() const { return static_cast<int>(cfg_.max_context); }

    // Hapus riwayat di RAM dan database. Fakta tidak disentuh.
    void clear();

private:
    void trim_history();
    void save_turn(const std::string& user, const std::string& reply);
    std::string build_system_prompt();

    static constexpr int kMaxToolRounds = 5;

    const Config& cfg_;
    LLMBackend& backend_;
    Memory& memory_;
    ToolRegistry& tools_;
    std::vector<Message> history_;  // indeks 0 selalu system prompt

    std::atomic<int> ctx_used_{-1};        // dibaca thread UI, ditulis thread kerja
    std::atomic<bool> ctx_estimated_{false};
};