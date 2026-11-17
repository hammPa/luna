#include "search/tavily_provider.hpp"

#include <stdexcept>
#include <utility>
#include <nlohmann/json.hpp>

TavilyProvider::TavilyProvider(std::string api_key, std::string base_url, long timeout_sec)
    : api_key_(std::move(api_key)),
      base_url_(base_url.empty() ? "https://api.tavily.com" : std::move(base_url)),
      timeout_(timeout_sec) {
    while (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();
    if (base_url_.rfind("https://", 0) != 0)
        throw std::runtime_error("search.base_url harus https");
}

std::vector<SearchResult> TavilyProvider::search(const std::string& query, int max_results) {
    nlohmann::json body = {
        {"query", query},
        {"max_results", max_results},
        {"search_depth", "basic"}
    };
    const HttpResponse r = http_.post(
        base_url_ + "/search",
        {"Authorization: Bearer " + api_key_, "Content-Type: application/json"},
        body.dump(), timeout_);

    if (!r.ok())
        throw std::runtime_error("Tavily HTTP " + std::to_string(r.statusCode));

    auto j = nlohmann::json::parse(r.body, nullptr, false);
    if (j.is_discarded() || !j.contains("results") || !j["results"].is_array())
        throw std::runtime_error("respons Tavily tidak valid");

    std::vector<SearchResult> out;
    for (const auto& it : j["results"]) {
        if (!it.is_object()) continue;
        SearchResult s;
        if (it.contains("title") && it["title"].is_string()) s.title = it["title"].get<std::string>();
        if (it.contains("url") && it["url"].is_string()) s.url = it["url"].get<std::string>();
        if (it.contains("content") && it["content"].is_string()) s.snippet = it["content"].get<std::string>();
        if (!s.url.empty()) out.push_back(std::move(s));
    }
    return out;
}