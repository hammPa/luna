#pragma once

#include "utils/markdown.hpp"

#include <string>

namespace md {

inline std::string inline_ansi(const std::vector<Span>& spans) {
    std::string o;
    for (const auto& sp : spans) {
        switch (sp.kind) {
            case SpanKind::Bold:   o += "\x1b[1m" + sp.text + "\x1b[22m"; break;
            case SpanKind::Italic: o += "\x1b[3m" + sp.text + "\x1b[23m"; break;
            case SpanKind::Code:   o += "\x1b[33m" + sp.text + "\x1b[39m"; break;
            case SpanKind::Link:
                o += "\x1b[4;34m" + sp.text + "\x1b[24;39m";
                if (!sp.url.empty()) o += " \x1b[2m(" + sp.url + ")\x1b[22m";
                break;
            case SpanKind::Text:   o += sp.text; break;
        }
    }
    return o;
}

// Hanya untuk stdout terminal. Untuk pipe, cetak teks asli.
// Karakter kontrol dari model sudah dibuang oleh parse().
inline std::string to_ansi(const std::string& text) {
    std::string o;
    for (const auto& b : parse(text)) {
        const std::string indent(static_cast<std::size_t>(b.level) * 2, ' ');
        switch (b.kind) {
            case BlockKind::Paragraph: o += inline_ansi(b.spans) + "\n"; break;
            case BlockKind::Heading:   o += "\x1b[1;35m" + inline_ansi(b.spans) + "\x1b[0m\n"; break;
            case BlockKind::Bullet:    o += indent + "• " + inline_ansi(b.spans) + "\n"; break;
            case BlockKind::Numbered:  o += indent + b.label + ". " + inline_ansi(b.spans) + "\n"; break;
            case BlockKind::Quote:     o += "\x1b[2m▎ \x1b[22m\x1b[3m" + inline_ansi(b.spans) + "\x1b[23m\n"; break;
            case BlockKind::Rule:      o += "\x1b[2m────────────────────\x1b[22m\n"; break;
            case BlockKind::Blank:     o += "\n"; break;
            case BlockKind::Code: {
                o += "\x1b[2m╭─ " + (b.label.empty() ? std::string("kode") : b.label) + "\x1b[22m\n";
                std::string line;
                for (std::size_t i = 0; i <= b.code.size(); ++i) {
                    if (i == b.code.size() || b.code[i] == '\n') {
                        o += "\x1b[2m│\x1b[22m \x1b[36m" + line + "\x1b[39m\n";
                        line.clear();
                    } else {
                        line += b.code[i];
                    }
                }
                o += "\x1b[2m╰─\x1b[22m\n";
                break;
            }
        }
    }
    while (!o.empty() && o.back() == '\n') o.pop_back();
    return o;
}

}  // namespace md