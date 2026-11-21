#pragma once

#include <string>
#include <vector>

namespace tui_text {

// Teks dari model atau web bisa berisi escape terminal. Buang karakter kontrol
// kecuali newline agar tidak merusak atau memanipulasi layar.
inline std::string sanitize(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        if (c == '\n') out += '\n';
        else if (c == '\t') out += ' ';
        else if (c < 0x20 || c == 0x7f) continue;
        else out += static_cast<char>(c);
    }
    return out;
}

inline std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

inline std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '\n') { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

}  // namespace tui_text