#include "llm/backend_factory.hpp"

#include "llm/openai_backend.hpp"

#include <cstdlib>
#include <stdexcept>

std::unique_ptr<LLMBackend> create_backend(const Config& cfg) {
    if (cfg.backend == "openai") {
        if (cfg.base_url.empty()) {
            throw std::runtime_error("Backend 'openai' butuh field 'base_url'");
        }

        std::string api_key;
        if (!cfg.api_key_env.empty()) {
            const char* v = std::getenv(cfg.api_key_env.c_str());
            if (v == nullptr || *v == '\0') {
                throw std::runtime_error("Environment variable " + cfg.api_key_env +
                                         " belum di-set");
            }
            api_key = v;
        }

        return std::make_unique<OpenAIBackend>(cfg.base_url, cfg.model, api_key,
                                               cfg.timeout_sec);
    }

    // Tempat backend berikutnya: "ollama", "llamacpp", dst.
    throw std::runtime_error("Backend tidak dikenal: '" + cfg.backend +
                             "'. Yang tersedia: openai");
}