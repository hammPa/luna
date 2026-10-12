#pragma once

#include "llm/llm_types.hpp"

#include <string>
#include <vector>

class LLMBackend {
public:
    virtual ~LLMBackend() = default;

    // Lempar std::runtime_error jika gagal (jaringan, HTTP error, balasan rusak).
    virtual LLMResponse chat(const std::vector<Message>& history,
                             const nlohmann::json& tools_schema) = 0;

    virtual std::string name() const = 0;
};