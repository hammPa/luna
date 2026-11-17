#include "search_factory.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "tavily_provider.hpp"

std::unique_ptr<SearchProvider> make_search_provider(const Config& cfg) {
    if (cfg.search_provider.empty()) return nullptr;

    const char* key = cfg.search_api_key_env.empty()
                          ? nullptr : std::getenv(cfg.search_api_key_env.c_str());

    if (cfg.search_provider == "tavily") {
        if (!key || !*key) {
            std::cerr << "[search] env " << cfg.search_api_key_env
                      << " kosong, search_web dimatikan\n";
            return nullptr;
        }
        return std::make_unique<TavilyProvider>(key, cfg.search_base_url, 20L);
    }
    throw std::runtime_error("search.provider tidak dikenal: " + cfg.search_provider);
}