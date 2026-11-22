#include "llm//openai/openai_backend.hpp"
#include "llm/openai/openai_http.hpp"
#include "llm/openai/openai_format.hpp"

#include "utils/cancel.hpp"
#include "utils/log.hpp"

#include <atomic>
#include <stdexcept>
#include <string>
#include <utility>

using json = nlohmann::json;
using namespace openai;

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