#pragma once

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace cfgload {

// Tolak field model tingkat atas format lama (sebelum 1.5).
inline void reject_old_fields(const nlohmann::json& j) {
    for (const char* old : {"backend", "model", "base_url", "api_key_env",
                            "max_context", "timeout_sec", "history_limit"}) {
        if (j.contains(old)) {
            throw std::runtime_error(std::string("field '") + old +
                "' tingkat atas tidak dipakai lagi sejak 1.5; pindahkan ke 'providers' "
                "(lihat config.example.json)");
        }
    }
}

}  // namespace cfgload