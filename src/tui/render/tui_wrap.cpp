#include "tui/render/tui_wrap.hpp"

#include "tui/render/theme.hpp"
#include "tui/render/tui_utf8.hpp"

#include <algorithm>

using namespace ftxui;

namespace {

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

}  // namespace

Elements wrap_spans(const std::vector<md::Span>& spans, bool strong, Color base, int width,
                    const std::string& prefix_first, const std::string& prefix_rest,
                    Color prefix_color) {
    return wrap_tokens(tokenize(spans, strong), base, width, prefix_first, prefix_rest,
                       prefix_color);
}