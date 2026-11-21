#pragma once

#include "utils/config.hpp"

#include <nlohmann/json.hpp>

#include <vector>

namespace cfgload {

inline void read_telegram(const nlohmann::json& j, Config& cfg) {
    cfg.telegram_token_env = j.value("telegram_token_env", cfg.telegram_token_env);
    cfg.telegram_api_base_url = j.value("telegram_api_base_url", cfg.telegram_api_base_url);
    if (j.contains("telegram_allowed_users")) {
        cfg.telegram_allowed_users = j.at("telegram_allowed_users").get<std::vector<long long>>();
    }
}

}  // namespace cfgload