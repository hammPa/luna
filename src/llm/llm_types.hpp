#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

struct Toolcall {
    std::string id;
    std::string name;
    nlohmann::json arguments;
};

struct Message {
    std::string role;       // "system" | "user" | "assistant" | "tool"
    std::string content;
    std::string name;       // nama tool (untuk role "tool")
    std::string tool_call_id;          // untuk role "tool"
    std::vector<Toolcall> tool_calls;  // untuk role "assistant" yang meminta tool
};

inline Message make_msg(std::string role, std::string content) {
    Message m;
    m.role = std::move(role);
    m.content = std::move(content);
    return m;
}

struct LLMResponse {
    std::string text;
    std::vector<Toolcall> toolcalls;

    bool wants_toolcall() const {
        return !toolcalls.empty();
    }
};