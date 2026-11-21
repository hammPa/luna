#include "tui/render/tui_markdown.hpp"

#include "tui/render/theme.hpp"
#include "tui/render/tui_text.hpp"
#include "tui/render/tui_utf8.hpp"
#include "tui/render/tui_wrap.hpp"
#include "utils/markdown.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace ftxui;

namespace {

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
            return wrap_spans(b.spans, true, theme::accent, width, "", "", theme::accent);
        case md::BlockKind::Bullet: {
            const std::string p1 = indent + "• ";
            return wrap_spans(b.spans, false, body, width, p1,
                              std::string(text_width(p1), ' '), theme::accent);
        }
        case md::BlockKind::Numbered: {
            const std::string p1 = indent + b.label + ". ";
            return wrap_spans(b.spans, false, body, width, p1,
                              std::string(text_width(p1), ' '), theme::accent);
        }
        case md::BlockKind::Quote:
            return wrap_spans(b.spans, false, theme::muted, width, "▎ ", "▎ ", theme::muted);
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
    return wrap_spans(b.spans, false, body, width, "", "", body);
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