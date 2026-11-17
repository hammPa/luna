#pragma once
#include <string>
#include <vector>
#include "search/search_provider.hpp"
#include "utils/http.hpp"

class TavilyProvider : public SearchProvider {
public:
    TavilyProvider(std::string api_key, std::string base_url, long timeout_sec);
    std::vector<SearchResult> search(const std::string& query, int max_results) override;
    std::string name() const override { return "tavily"; }
private:
    std::string api_key_;
    std::string base_url_;
    long timeout_;
    HttpClient http_;
};