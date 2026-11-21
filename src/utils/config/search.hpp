#pragma once

#include "utils/config.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace cfgload {

inline void read_search(const nlohmann::json& j, Config& cfg) {
    if (!j.contains("search")) return;
    const auto& s = j.at("search");
    if (!s.is_object()) throw std::runtime_error("'search' harus object");
    cfg.search_provider    = s.value("provider", cfg.search_provider);
    cfg.search_api_key_env = s.value("api_key_env", cfg.search_api_key_env);
    cfg.search_base_url    = s.value("base_url", cfg.search_base_url);
    cfg.search_max_results = s.value("max_results", cfg.search_max_results);
    if (cfg.search_max_results < 1 || cfg.search_max_results > 10) {
        throw std::runtime_error("search.max_results harus 1 sampai 10");
    }
}

}  // namespace cfgload