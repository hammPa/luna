#pragma once

#include <algorithm>
#include <cstdio>
#include <string>

namespace ctx_fmt {

// Estimasi kasar ~3 byte/token (dipakai hanya jika provider tidak mengirim usage).
inline int estimate_tokens(std::size_t bytes) { return static_cast<int>((bytes + 2) / 3); }

inline std::string compact(int n) {
    if (n < 1000) return std::to_string(n);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.1fk", n / 1000.0);
    return buf;
}

inline int percent(int used, int max) {
    if (used < 0 || max <= 0) return -1;
    return std::min(999, static_cast<int>(static_cast<long long>(used) * 100 / max));
}

// 0 = aman (<60%), 1 = waspada (60-85%), 2 = penuh (>85%)
inline int level(int pct) { return pct < 60 ? 0 : (pct <= 85 ? 1 : 2); }

inline std::string bar(int pct, int cells = 5) {
    const int filled = (pct < 0) ? 0 : std::min(cells, (pct * cells + 99) / 100);
    std::string s;
    for (int i = 0; i < cells; ++i) s += (i < filled) ? "▰" : "▱";
    return s;
}

// "ctx ~1.2k / 4096"  atau  "ctx -" bila belum ada angka
inline std::string label(int used, int max, bool estimated) {
    if (used < 0) return "ctx -";
    return std::string("ctx ") + (estimated ? "~" : "") + compact(used) + " / " + std::to_string(max);
}

// "ctx ~1.2k / 4096 · 30%"
inline std::string summary(int used, int max, bool estimated) {
    std::string s = label(used, max, estimated);
    const int pct = percent(used, max);
    if (pct >= 0) s += " · " + std::to_string(pct) + "%";
    return s;
}

}  // namespace ctx_fmt