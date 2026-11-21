#pragma once

#include <cstdint>
#include <iostream>
#include <string>

namespace clipboard {

inline std::string base64(const std::string& in) {
    static const char* tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        const uint32_t v = (static_cast<uint8_t>(in[i]) << 16) |
                           (static_cast<uint8_t>(in[i + 1]) << 8) |
                            static_cast<uint8_t>(in[i + 2]);
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];
        out += tbl[v & 63];
    }
    if (i + 1 == in.size()) {
        const uint32_t v = static_cast<uint32_t>(static_cast<uint8_t>(in[i])) << 16;
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += "==";
    } else if (i + 2 == in.size()) {
        const uint32_t v = (static_cast<uint32_t>(static_cast<uint8_t>(in[i])) << 16) |
                           (static_cast<uint32_t>(static_cast<uint8_t>(in[i + 1])) << 8);
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];
        out += '=';
    }
    return out;
}

// Salin ke clipboard terminal lewat OSC 52. Tanpa subprocess.
inline bool copy(const std::string& s) {
    if (s.empty() || s.size() > 100000) return false;
    std::cout << "\x1b]52;c;" << base64(s) << '\a' << std::flush;
    return true;
}

}  // namespace clipboard