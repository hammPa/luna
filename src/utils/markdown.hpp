#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace md {

enum class SpanKind { Text, Bold, Italic, Code, Link };

struct Span {
    SpanKind kind = SpanKind::Text;
    std::string text;
    std::string url;   // hanya Link
};

enum class BlockKind { Paragraph, Heading, Bullet, Numbered, Quote, Code, Rule, Blank };

struct Block {
    BlockKind kind = BlockKind::Paragraph;
    int level = 0;            // Heading: 1-6; Bullet/Numbered: tingkat indentasi
    std::string label;        // Numbered: "1"; Code: nama bahasa
    std::vector<Span> spans;
    std::string code;         // isi blok kode
    std::string raw;          // teks asal, untuk cadangan polos
};

// Buang karakter kontrol (termasuk ESC) agar teks dari model tidak bisa
// memanipulasi terminal. Newline dipertahankan, tab jadi 4 spasi.
inline std::string strip_control(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        if (c == '\n') out += '\n';
        else if (c == '\t') out += "    ";
        else if (c < 0x20 || c == 0x7f) continue;
        else out += static_cast<char>(c);
    }
    return out;
}

inline std::string trim_sp(const std::string& s) {
    const auto b = s.find_first_not_of(' ');
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(' ') - b + 1);
}

// Tanda yang tidak berpasangan dibiarkan sebagai teks biasa.
inline std::vector<Span> parse_inline(const std::string& s) {
    std::vector<Span> out;
    std::string buf;
    auto flush = [&] {
        if (!buf.empty()) { out.push_back({SpanKind::Text, buf, ""}); buf.clear(); }
    };
    const std::size_t n = s.size();
    std::size_t i = 0;
    while (i < n) {
        const char c = s[i];
        if (c == '`') {
            const std::size_t e = s.find('`', i + 1);
            if (e != std::string::npos && e > i + 1) {
                flush();
                out.push_back({SpanKind::Code, s.substr(i + 1, e - i - 1), ""});
                i = e + 1;
                continue;
            }
        } else if (c == '*' && i + 1 < n && s[i + 1] == '*') {
            const std::size_t e = s.find("**", i + 2);
            if (e != std::string::npos && e > i + 2) {
                flush();
                out.push_back({SpanKind::Bold, s.substr(i + 2, e - i - 2), ""});
                i = e + 2;
                continue;
            }
        } else if (c == '*' && i + 1 < n && s[i + 1] != ' ' && s[i + 1] != '*') {
            const std::size_t e = s.find('*', i + 1);
            if (e != std::string::npos && e > i + 1 && s[e - 1] != ' ') {
                flush();
                out.push_back({SpanKind::Italic, s.substr(i + 1, e - i - 1), ""});
                i = e + 1;
                continue;
            }
        } else if (c == '[') {
            const std::size_t mid = s.find("](", i + 1);
            if (mid != std::string::npos && mid > i + 1) {
                const std::size_t end = s.find(')', mid + 2);
                if (end != std::string::npos && end > mid + 2) {
                    flush();
                    out.push_back({SpanKind::Link, s.substr(i + 1, mid - i - 1),
                                   s.substr(mid + 2, end - mid - 2)});
                    i = end + 1;
                    continue;
                }
            }
        }
        buf += c;
        ++i;
    }
    flush();
    return out;
}

namespace detail {

// Baris pagar kode: ``` atau ```bahasa. lang diisi kata pertama setelahnya.
inline bool is_fence(const std::string& line, std::string* lang) {
    const std::size_t p = line.find_first_not_of(' ');
    if (p == std::string::npos || line.compare(p, 3, "```") != 0) return false;
    const std::string rest = trim_sp(line.substr(p + 3));
    if (rest.find('`') != std::string::npos) return false;   // ```kode``` satu baris
    if (lang) *lang = rest.substr(0, rest.find(' '));
    return true;
}

inline bool is_rule(const std::string& b) {
    if (b.size() < 3) return false;
    const char c = b[0];
    if (c != '-' && c != '*' && c != '_') return false;
    for (char x : b) if (x != c) return false;
    return true;
}

}  // namespace detail

inline std::vector<Block> parse(const std::string& input) {
    std::vector<std::string> lines;
    {
        const std::string text = strip_control(input);
        std::string cur;
        for (char c : text) {
            if (c == '\n') { lines.push_back(cur); cur.clear(); }
            else cur += c;
        }
        lines.push_back(cur);
    }

    std::vector<Block> out;
    auto push_blank = [&] {
        if (!out.empty() && out.back().kind != BlockKind::Blank) {
            Block b; b.kind = BlockKind::Blank; out.push_back(b);
        }
    };

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string& ln = lines[i];

        std::string lang;
        if (detail::is_fence(ln, &lang)) {
            Block b;
            b.kind = BlockKind::Code;
            b.label = lang;
            std::string code;
            bool first = true;
            ++i;
            while (i < lines.size() && !detail::is_fence(lines[i], nullptr)) {
                if (!first) code += '\n';
                code += lines[i];
                first = false;
                ++i;
            }
            b.code = code;
            b.raw = code;
            out.push_back(std::move(b));
            continue;   // ++i milik for melewati pagar penutup
        }

        const std::size_t ind = ln.find_first_not_of(' ');
        if (ind == std::string::npos) { push_blank(); continue; }
        const std::string body = ln.substr(ind);

        Block b;
        b.raw = ln;

        std::size_t h = 0;
        while (h < body.size() && body[h] == '#') ++h;
        std::size_t d = 0;
        while (d < body.size() && std::isdigit(static_cast<unsigned char>(body[d]))) ++d;

        if (h >= 1 && h <= 6 && h < body.size() && body[h] == ' ') {
            b.kind = BlockKind::Heading;
            b.level = static_cast<int>(h);
            b.spans = parse_inline(trim_sp(body.substr(h + 1)));
        } else if (detail::is_rule(body)) {
            b.kind = BlockKind::Rule;
        } else if (body[0] == '>') {
            b.kind = BlockKind::Quote;
            std::string rest = body.substr(1);
            if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
            b.spans = parse_inline(rest);
        } else if (body.size() >= 2 && (body[0] == '-' || body[0] == '*' || body[0] == '+') &&
                   body[1] == ' ') {
            b.kind = BlockKind::Bullet;
            b.level = static_cast<int>(ind / 2);
            b.spans = parse_inline(trim_sp(body.substr(2)));
        } else if (d > 0 && d <= 9 && d + 1 < body.size() && body[d] == '.' && body[d + 1] == ' ') {
            b.kind = BlockKind::Numbered;
            b.level = static_cast<int>(ind / 2);
            b.label = body.substr(0, d);
            b.spans = parse_inline(trim_sp(body.substr(d + 2)));
        } else {
            b.kind = BlockKind::Paragraph;
            b.spans = parse_inline(body);
        }
        out.push_back(std::move(b));
    }
    while (!out.empty() && out.back().kind == BlockKind::Blank) out.pop_back();
    return out;
}

}  // namespace md