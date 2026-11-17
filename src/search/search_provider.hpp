#pragma once
#include <string>
#include <vector>

struct SearchResult {
    std::string title;
    std::string url;
    std::string snippet;
};

class SearchProvider {
public:
    virtual ~SearchProvider() = default;
    // Melempar std::runtime_error jika gagal.
    virtual std::vector<SearchResult> search(const std::string& query, int max_results) = 0;
    virtual std::string name() const = 0;
};