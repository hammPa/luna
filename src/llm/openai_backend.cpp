#include "llm/openai_backend.hpp"

#include <stdexcept>
#include <utility>

using json = nlohmann::json;

namespace {

// Pesan error dari server. Format Mistral: {"message": "..."}
// Format OpenAI: {"error": {"message": "..."}}
std::string server_message(const std::string& body) {
    try {
        json j = json::parse(body);
        if (j.contains("error") && j["error"].is_object() &&
            j["error"].contains("message") && j["error"]["message"].is_string()) {
            return j["error"]["message"].get<std::string>();
        }
        if (j.contains("message") && j["message"].is_string()) {
            return j["message"].get<std::string>();
        }
    } catch (const json::exception&) {
        // body bukan JSON, abaikan
    }
    return body;
}

std::string describe_http_error(const HttpResponse& res) {
    std::string detail = server_message(res.body);
    switch (res.statusCode) {
        case 401: return "API key salah atau tidak valid (401): " + detail;
        case 429: return "Kena rate limit (429), coba lagi sebentar: " + detail;
        default:  return "HTTP " + std::to_string(res.statusCode) + ": " + detail;
    }
}

// "content" biasanya string, tapi sebagian API mengirim array potongan.
std::string extract_text(const json& content) {
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
json to_openai_tools(const json& schema) {
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
json to_openai_message(const Message& m) {
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
                {"function", {{"name", c.name}, {"arguments", c.arguments.dump()}}},
            });
        }
        item["tool_calls"] = std::move(calls);
    }
    return item;
}

// Balasan OpenAI -> vector<Toolcall>. Tidak pernah melempar karena JSON rusak.
std::vector<Toolcall> parse_tool_calls(const json& msg) {
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

}  // namespace

OpenAIBackend::OpenAIBackend(std::string base_url,
                             std::string model,
                             std::string api_key,
                             long timeout_sec)
    : model_(std::move(model)), timeout_sec_(timeout_sec) {
    while (!base_url.empty() && base_url.back() == '/') {
        base_url.pop_back();
    }
    url_ = base_url + "/chat/completions";
    headers_ = {
        "Content-Type: application/json",
        "Authorization: Bearer " + api_key,
    };
}

LLMResponse OpenAIBackend::chat(const std::vector<Message>& history,
                                const nlohmann::json& tools_schema) {
    json messages = json::array();
    for (const auto& m : history) {
        messages.push_back(to_openai_message(m));
    }

    json body = {
        {"model", model_},
        {"messages", std::move(messages)},
        {"stream", false},
    };
    if (tools_schema.is_array() && !tools_schema.empty()) {
        body["tools"] = to_openai_tools(tools_schema);
    }

    HttpResponse res = http_.post(url_, headers_, body.dump(), timeout_sec_);
    if (!res.ok()) {
        throw std::runtime_error(describe_http_error(res));
    }

    try {
        json j = json::parse(res.body);
        const json& choices = j.at("choices");
        if (!choices.is_array() || choices.empty()) {
            throw std::runtime_error("Balasan tanpa 'choices'");
        }
        const json& msg = choices[0].at("message");

        LLMResponse out;
        if (msg.contains("content")) {
            out.text = extract_text(msg["content"]);
        }
        out.toolcalls = parse_tool_calls(msg);
        return out;
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("Balasan tidak sesuai format: ") + e.what());
    }
}