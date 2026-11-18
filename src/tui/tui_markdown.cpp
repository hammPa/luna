#include "tui/tui_markdown.hpp"

#include "tui/theme.hpp"
#include "tui/tui_text.hpp"
#include "utils/markdown.hpp"

using namespace ftxui;

namespace {

Element styled(const std::string& tok, md::SpanKind kind, Color base, bool strong) {
    Element e = text(tok);
    switch (kind) {
        case md::SpanKind::Bold:   e = e | bold | color(base); break;
        case md::SpanKind::Italic: e = e | italic | color(base); break;
        case md::SpanKind::Link:   e = e | underlined | color(theme::link); break;
        default:                   e = e | color(base); break;
    }
    if (strong) e = e | bold;
    return e;
}

// Satu elemen per kata (plus spasi sesudahnya) supaya hflow bisa membungkus baris.
Elements inline_elems(const std::vector<md::Span>& spans, Color base, bool strong) {
    Elements out;
    for (const auto& sp : spans) {
        if (sp.kind == md::SpanKind::Code) {
            out.push_back(text(" " + sp.text + " ") | color(theme::inline_code) |
                          bgcolor(theme::code_bg));
            continue;
        }
        const std::string& s = sp.text;
        std::size_t i = 0;
        while (i < s.size()) {
            std::size_t j = i;
            while (j < s.size() && s[j] != ' ') ++j;
            std::size_t k = j;
            while (k < s.size() && s[k] == ' ') ++k;
            out.push_back(styled(s.substr(i, j - i) + (k > j ? " " : ""), sp.kind, base, strong));
            i = k;
        }
        if (sp.kind == md::SpanKind::Link && !sp.url.empty()) {
            out.push_back(text(" (" + sp.url + ") ") | color(theme::muted));
        }
    }
    if (out.empty()) out.push_back(text(""));
    return out;
}

Element code_block(const md::Block& b) {
    Elements rows;
    if (!b.label.empty()) {
        rows.push_back(hbox({text(" " + b.label + " ") | bold | color(theme::accent), filler()}) |
                       bgcolor(theme::code_bg));
    }
    for (const auto& ln : tui_text::split_lines(b.code)) {
        rows.push_back(hbox({text(" " + ln + " ") | color(theme::fg), filler()}) |
                       bgcolor(theme::code_bg));
    }
    return vbox(std::move(rows));
}

Element render_block(const md::Block& b, Color body) {
    const std::string indent(static_cast<std::size_t>(b.level) * 2, ' ');
    switch (b.kind) {
        case md::BlockKind::Heading:
            return hflow(inline_elems(b.spans, theme::accent, true));
        case md::BlockKind::Bullet:
            return hbox({text(indent + "• ") | color(theme::accent),
                         hflow(inline_elems(b.spans, body, false)) | flex});
        case md::BlockKind::Numbered:
            return hbox({text(indent + b.label + ". ") | color(theme::accent),
                         hflow(inline_elems(b.spans, body, false)) | flex});
        case md::BlockKind::Quote:
            return hbox({text("▎ ") | color(theme::muted),
                         hflow(inline_elems(b.spans, theme::muted, false)) | flex});
        case md::BlockKind::Rule:
            return separatorLight() | color(theme::muted);
        case md::BlockKind::Blank:
            return text("");
        case md::BlockKind::Code:
            return code_block(b);
        case md::BlockKind::Paragraph:
            break;
    }
    return hflow(inline_elems(b.spans, body, false));
}

}  // namespace

Elements markdown_rows(const std::string& content, Color body) {
    Elements rows;
    for (const auto& b : md::parse(content)) rows.push_back(render_block(b, body));
    if (rows.empty()) rows.push_back(text(""));
    return rows;
}