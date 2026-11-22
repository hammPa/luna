#pragma once

// Konversi format internal <-> format OpenAI. Hanya OpenAIBackend yang boleh memakainya.

#include "llm/llm_types.hpp"

#include <string>
#include <utility>
#include <vector>

namespace openai {

using json = nlohmann::json;

// "content" biasanya string, tapi sebagian API mengirim array potongan.
inline std::string extract_text(const json& content) {
    if (content.is_string()) {
        return content.get<std::string>();
    }
    if (content.is_array()) {
        std::string out;
        for (const auto& part : content) {
            if (part.is_object() && part.contains("text") && part["text"].is_string()) {
                out += part["text"].get<std::string>();
            }
        }
        return out;
    }
    return {};  // null atau tipe lain
}

// Format internal {name, description, parameters} -> format OpenAI.
inline json to_openai_tools(const json& schema) {
    json tools = json::array();
    for (const auto& t : schema) {
        tools.push_back({
            {"type", "function"},
            {"function", {
                {"name", t.at("name")},
                {"description", t.at("description")},
                {"parameters", t.at("parameters")},
            }},
        });
    }
    return tools;
}

// Message internal -> JSON OpenAI.
inline json to_openai_message(const Message& m) {
    json item = {{"role", m.role}, {"content", m.content}};

    if (m.role == "tool") {
        item["tool_call_id"] = m.tool_call_id;
    } else if (!m.name.empty()) {
        item["name"] = m.name;
    }

    if (m.role == "assistant" && !m.tool_calls.empty()) {
        json calls = json::array();
        for (const auto& c : m.tool_calls) {
            calls.push_back({
                {"id", c.id},
                {"type", "function"},
                // OpenAI minta arguments berupa STRING JSON, bukan objek.
                {"function", {{"name", c.name}, {"arguments", c.arguments.dump(-1, ' ', false, json::error_handler_t::replace)}}},
            });
        }
        item["tool_calls"] = std::move(calls);
    }
    return item;
}

// Balasan OpenAI -> vector<Toolcall>. Tidak pernah melempar karena JSON rusak.
inline std::vector<Toolcall> parse_tool_calls(const json& msg) {
    std::vector<Toolcall> out;
    if (!msg.contains("tool_calls") || !msg["tool_calls"].is_array()) {
        return out;  // null atau tidak ada = tidak ada tool call
    }
    for (const auto& c : msg["tool_calls"]) {
        if (!c.is_object() || !c.contains("function")) continue;
        const json& fn = c["function"];

        Toolcall tc;
        tc.id = c.value("id", "");
        tc.name = fn.value("name", "");
        if (tc.name.empty()) continue;

        tc.arguments = json::object();
        if (fn.contains("arguments")) {
            const json& a = fn["arguments"];
            if (a.is_string()) {
                try {
                    json parsed = json::parse(a.get<std::string>());
                    if (parsed.is_object()) tc.arguments = std::move(parsed);
                } catch (const json::exception&) {
                    // Model mengirim JSON rusak. Args tetap kosong;
                    // tool akan gagal validasi dan model dapat pesan error.
                }
            } else if (a.is_object()) {
                tc.arguments = a;  // sebagian provider mengirim objek langsung
            }
        }
        out.push_back(std::move(tc));
    }
    return out;
}

}  // namespace openai