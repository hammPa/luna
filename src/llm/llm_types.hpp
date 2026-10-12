#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

struct Message {
    std::string role;
    std::string content;
    std::string name;
};

struct Toolcall {
    std::string id;
    std::string name;
    nlohmann::json arguments;
};

struct LLMResponse {
    std::string text;
    std::vector<Toolcall> toolcalls;

    bool wants_toolcall() const {
        return !toolcalls.empty();
    }
};