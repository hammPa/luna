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
                                const nlohmann::json& /*tools_schema*/) {
    // 1. Format internal -> format OpenAI
    json messages = json::array();
    for (const auto& m : history) {
        json item = {{"role", m.role}, {"content", m.content}};
        if (!m.name.empty()) {
            item["name"] = m.name;
        }
        messages.push_back(std::move(item));
    }

    json body = {
        {"model", model_},
        {"messages", std::move(messages)},
        {"stream", false},
    };

    // 2. Kirim. Jaringan gagal = exception dari HttpClient.
    HttpResponse res = http_.post(url_, headers_, body.dump(), timeout_sec_);
    if (!res.ok()) {
        throw std::runtime_error(describe_http_error(res));
    }

    // 3. Format OpenAI -> format internal
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
        // tool_calls sengaja diabaikan di Fase 1 (nilainya bisa null)
        return out;
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("Balasan tidak sesuai format: ") + e.what());
    }
}