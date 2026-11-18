#include "tui/tui_markdown.hpp"

#include "tui/theme.hpp"
#include "tui/tui_text.hpp"
#include "utils/markdown.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace ftxui;

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

// Pecah string UTF-8 jadi potongan per codepoint (string asli + lebarnya).
struct Glyph { std::string s; int w; };

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

// Token berstyle: satu kata (atau potongan kata panjang) dengan gayanya.
struct Tok {
    std::string text;
    md::SpanKind kind = md::SpanKind::Text;
    bool strong = false;
    bool pad = false;      // kode inline: diberi spasi tepi dan latar
    bool space_after = false;
    int w = 0;
};

Element tok_element(const Tok& t, Color base) {
    std::string s = t.text;
    if (t.pad) s = " " + s + " ";
    Element e = text(s);
    switch (t.kind) {
        case md::SpanKind::Bold:   e = e | bold | color(base); break;
        case md::SpanKind::Italic: e = e | italic | color(base); break;
        case md::SpanKind::Link:   e = e | underlined | color(theme::link); break;
        case md::SpanKind::Code:   e = e | color(theme::inline_code) | bgcolor(theme::code_bg); break;
        default:                   e = e | color(base); break;
    }
    if (t.strong && t.kind != md::SpanKind::Code) e = e | bold;
    return e;
}

std::vector<Tok> tokenize(const std::vector<md::Span>& spans, bool strong) {
    std::vector<Tok> out;
    for (const auto& sp : spans) {
        if (sp.kind == md::SpanKind::Code) {
            Tok t;
            t.text = sp.text;
            t.kind = md::SpanKind::Code;
            t.pad = true;
            t.w = text_width(sp.text) + 2;
            out.push_back(t);
            continue;
        }
        const std::string& s = sp.text;
        std::size_t i = 0;
        while (i < s.size()) {
            std::size_t j = i;
            while (j < s.size() && s[j] != ' ') ++j;
            std::size_t k = j;
            while (k < s.size() && s[k] == ' ') ++k;
            Tok t;
            t.text = s.substr(i, j - i);
            t.kind = sp.kind;
            t.strong = strong;
            t.space_after = (k > j);
            t.w = text_width(t.text);
            if (!t.text.empty()) out.push_back(t);
            else if (k > j && !out.empty()) out.back().space_after = true;
            i = k;
        }
        if (sp.kind == md::SpanKind::Link && !sp.url.empty()) {
            Tok t;
            t.text = "(" + sp.url + ")";
            t.kind = md::SpanKind::Text;
            t.w = text_width(t.text);
            t.space_after = true;
            out.push_back(t);
        }
    }
    return out;
}

// Potong token yang lebih lebar dari satu baris menjadi beberapa token.
std::vector<Tok> split_long(const std::vector<Tok>& in, int width) {
    std::vector<Tok> out;
    for (const auto& t : in) {
        if (t.w <= width || t.pad) { out.push_back(t); continue; }
        Tok cur = t;
        cur.text.clear();
        cur.w = 0;
        cur.space_after = false;
        for (const auto& g : glyphs(t.text)) {
            if (cur.w + g.w > width && cur.w > 0) {
                out.push_back(cur);
                cur.text.clear();
                cur.w = 0;
            }
            cur.text += g.s;
            cur.w += g.w;
        }
        cur.space_after = t.space_after;
        if (!cur.text.empty()) out.push_back(cur);
    }
    return out;
}

// Bungkus token menjadi baris. prefix_first dan prefix_rest = awalan tiap baris
// (mis. "• " lalu spasi selebar itu); lebarnya sama.
Elements wrap_tokens(const std::vector<Tok>& raw, Color base, int width,
                     const std::string& prefix_first, const std::string& prefix_rest,
                     Color prefix_color) {
    const int pw = text_width(prefix_first);
    const int avail = std::max(4, width - pw);
    const std::vector<Tok> toks = split_long(raw, avail);

    Elements rows;
    Elements cur;
    int used = 0;
    bool first = true;
    bool pending_space = false;

    auto flush = [&] {
        const std::string& pre = first ? prefix_first : prefix_rest;
        Elements line;
        line.push_back(text(pre) | color(prefix_color));
        for (auto& e : cur) line.push_back(std::move(e));
        rows.push_back(hbox(std::move(line)));
        cur.clear();
        used = 0;
        first = false;
        pending_space = false;
    };

    for (const auto& t : toks) {
        const int need = t.w + (pending_space && used > 0 ? 1 : 0);
        if (used > 0 && used + need > avail) flush();
        if (pending_space && used > 0) { cur.push_back(text(" ")); ++used; }
        cur.push_back(tok_element(t, base));
        used += t.w;
        pending_space = t.space_after;
    }
    if (!cur.empty() || rows.empty()) flush();
    return rows;
}

Elements code_rows(const md::Block& b, int width) {
    Elements rows;
    const int inner = std::max(4, width - 2);   // spasi tepi kiri dan kanan
    auto line = [&](const std::string& s, Color c, bool bld) {
        Element e = text(" " + s + " ") | color(c);
        if (bld) e = e | bold;
        rows.push_back(hbox({e, filler()}) | bgcolor(theme::code_bg));
    };
    if (!b.label.empty()) line(b.label, theme::accent, true);
    for (const auto& ln : tui_text::split_lines(b.code)) {
        // Baris kode dibungkus keras per kolom (tanpa menjaga kata).
        std::string cur;
        int w = 0;
        bool any = false;
        for (const auto& g : glyphs(ln)) {
            if (w + g.w > inner && w > 0) {
                line(cur, theme::fg, false);
                cur.clear();
                w = 0;
            }
            cur += g.s;
            w += g.w;
            any = true;
        }
        if (any || ln.empty()) line(cur, theme::fg, false);
    }
    return rows;
}

Elements block_rows(const md::Block& b, Color body, int width) {
    const std::string indent(static_cast<std::size_t>(b.level) * 2, ' ');
    switch (b.kind) {
        case md::BlockKind::Heading:
            return wrap_tokens(tokenize(b.spans, true), theme::accent, width, "", "",
                               theme::accent);
        case md::BlockKind::Bullet: {
            const std::string p1 = indent + "• ";
            return wrap_tokens(tokenize(b.spans, false), body, width, p1,
                               std::string(text_width(p1), ' '), theme::accent);
        }
        case md::BlockKind::Numbered: {
            const std::string p1 = indent + b.label + ". ";
            return wrap_tokens(tokenize(b.spans, false), body, width, p1,
                               std::string(text_width(p1), ' '), theme::accent);
        }
        case md::BlockKind::Quote:
            return wrap_tokens(tokenize(b.spans, false), theme::muted, width, "▎ ", "▎ ",
                               theme::muted);
        case md::BlockKind::Rule:
            return {text(std::string(static_cast<std::size_t>(std::max(3, width)), '-')) |
                    color(theme::muted)};
        case md::BlockKind::Blank:
            return {text("")};
        case md::BlockKind::Code:
            return code_rows(b, width);
        case md::BlockKind::Paragraph:
            break;
    }
    return wrap_tokens(tokenize(b.spans, false), body, width, "", "", body);
}

}  // namespace

Elements markdown_rows(const std::string& content, Color body, int width) {
    Elements rows;
    for (const auto& b : md::parse(content)) {
        for (auto& r : block_rows(b, body, width)) rows.push_back(std::move(r));
    }
    if (rows.empty()) rows.push_back(text(""));
    return rows;
}