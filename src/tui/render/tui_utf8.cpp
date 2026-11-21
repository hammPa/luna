#include "tui/render/tui_utf8.hpp"

#include <cstddef>

namespace {

// Lebar tampil satu codepoint UTF-8 (kasar: CJK dan emoji = 2, sisanya 1).
int cp_width(unsigned cp) {
    if (cp < 0x20) return 0;
    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0x1F300 && cp <= 0x1FAFF)) {
        return 2;
    }
    if (cp >= 0x0300 && cp <= 0x036F) return 0;   // tanda gabung
    return 1;
}

}  // namespace

std::vector<Glyph> glyphs(const std::string& in) {
    std::vector<Glyph> out;
    std::size_t i = 0;
    while (i < in.size()) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        std::size_t len = 1;
        unsigned cp = c;
        if (c >= 0xF0 && i + 3 < in.size() + 0) { len = 4; cp = c & 0x07; }
        else if (c >= 0xE0) { len = 3; cp = c & 0x0F; }
        else if (c >= 0xC0) { len = 2; cp = c & 0x1F; }
        if (i + len > in.size()) len = 1;
        for (std::size_t k = 1; k < len; ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(in[i + k]) & 0x3F);
        }
        out.push_back({in.substr(i, len), cp_width(cp)});
        i += len;
    }
    return out;
}

int text_width(const std::string& s) {
    int w = 0;
    for (const auto& g : glyphs(s)) w += g.w;
    return w;
}