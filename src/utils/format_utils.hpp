#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

// "512 B", "1.5 KB", "2.1 MB", "1.2 GB"
inline std::string human_size(std::uintmax_t bytes) {
    char buf[32];
    const double b = static_cast<double>(bytes);
    if (bytes < 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    } else if (bytes < 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.1f KB", b / 1024.0);
    } else if (bytes < 1024ULL * 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.1f MB", b / (1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f GB", b / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}
