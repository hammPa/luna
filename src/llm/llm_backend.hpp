#pragma once

#include "llm/llm_types.hpp"

#include <string>
#include <vector>
#include <atomic>

class LLMBackend {
public:
    virtual ~LLMBackend() = default;

    // Lempar std::runtime_error jika gagal (jaringan, HTTP error, balasan rusak).
    virtual LLMResponse chat(const std::vector<Message>& history,
                             const nlohmann::json& tools_schema) = 0;

    virtual std::string name() const = 0;
    
    // Bendera batal; bila menyala, permintaan yang sedang menunggu diputus.
    virtual void set_cancel_flag(const std::atomic<bool>*) {}
};