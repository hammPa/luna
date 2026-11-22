#include "llm/openai_backend.hpp"

#include "utils/cancel.hpp"
#include "utils/log.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
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

constexpr int kMaxRetries = 2;
constexpr double kMaxWaitSec = 20.0;

// 429, atau 503 dari 9router yang membungkus 429 milik Groq.
bool is_rate_limited(const HttpResponse& res) {
    if (res.statusCode == 429) return true;
    if (res.statusCode != 503) return false;
    std::string b = res.body;
    std::transform(b.begin(), b.end(), b.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return b.find("429") != std::string::npos ||
           b.find("rate limit") != std::string::npos ||
           b.find("rate_limit") != std::string::npos ||
           b.find("try again in") != std::string::npos;
}

// Baca "try again in 6.57s" / "1m30.5s" / "250ms". Hasil detik, -1 kalau tidak ada.
double parse_retry_after(const std::string& msg) {
    const std::string key = "try again in ";
    const std::size_t pos = msg.find(key);
    if (pos == std::string::npos) return -1;
    const char* p = msg.c_str() + pos + key.size();
    double total = 0;
    bool any = false;
    while (true) {
        char* end = nullptr;
        const double v = std::strtod(p, &end);
        if (end == p) break;
        if (end[0] == 'm' && end[1] == 's') { total += v / 1000.0; p = end + 2; }
        else if (end[0] == 'm')             { total += v * 60.0;   p = end + 1; }
        else if (end[0] == 's')             { total += v;          p = end + 1; }
        else break;
        any = true;
    }
    return any ? total : -1;
}

// Tidur per 100 ms supaya Esc/Ctrl+C tetap terasa. false = dibatalkan.
bool sleep_cancellable(double sec, const std::atomic<bool>* cancel) {
    const auto end = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(static_cast<long long>(sec * 1000));
    while (std::chrono::steady_clock::now() < end) {
        if (cancel && cancel->load()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return !(cancel && cancel->load());
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
                {"function", {{"name", c.name}, {"arguments", c.arguments.dump(-1, ' ', false, json::error_handler_t::replace)}}},
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

        const std::string payload = body.dump(-1, ' ', false, json::error_handler_t::replace);

    HttpResponse res;
    for (int attempt = 0;; ++attempt) {
        res = http_.post(url_, headers_, payload, timeout_sec_);
        if (res.ok() || !is_rate_limited(res) || attempt >= kMaxRetries) break;

        const double hint = parse_retry_after(server_message(res.body));
        const double wait = hint >= 0 ? hint + 0.5 : 2.0 * (attempt + 1);
        if (wait > kMaxWaitSec) {
            logger::warn("llm", "rate limit, tunggu " + std::to_string(static_cast<int>(wait)) +
                                " dtk melebihi batas, menyerah");
            break;
        }
        logger::warn("llm", "rate limit, coba ulang " + std::to_string(attempt + 1) + "/" +
                            std::to_string(kMaxRetries) + " setelah " +
                            std::to_string(static_cast<int>(wait)) + " dtk");
                if (notice_) {
            notice_("kena rate limit, coba ulang " + std::to_string(attempt + 1) + "/" +
                    std::to_string(kMaxRetries) + " dalam " +
                    std::to_string(static_cast<int>(wait)) + " dtk...");
        }
        const bool waited = sleep_cancellable(wait, cancel_);
        if (notice_) notice_("");
        if (!waited) {
            throw CancelledError();
        }
    }
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

        // token usage: OpenAI mengirim "usage" di level root, Mistral di level choices[0].
        if (j.contains("usage") && j["usage"].is_object()) {
            const json& u = j["usage"];
            if (u.contains("prompt_tokens") && u["prompt_tokens"].is_number_integer()) {
                const int p = u["prompt_tokens"].get<int>();
                if (p > 0) out.prompt_tokens = p;   // 0 dianggap "tidak diketahui"
            }
        }

        return out;
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("Balasan tidak sesuai format: ") + e.what());
    }
}