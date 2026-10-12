#pragma once

#include "llm/llm_backend.hpp"
#include "utils/http.hpp"

class OpenAIBackend : public LLMBackend {
public:
    OpenAIBackend(std::string base_url,
                  std::string model,
                  std::string api_key,
                  long timeout_sec = 30);

    LLMResponse chat(const std::vector<Message>& history,
                     const nlohmann::json& tools_schema) override;

    std::string name() const override { return "openai"; }

private:
    HttpClient http_;
    std::string url_;
    std::string model_;
    std::vector<std::string> headers_;
    long timeout_sec_;
};