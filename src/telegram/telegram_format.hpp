#pragma once

#include "telegram/telegram_client.hpp"   // split_message
#include "utils/markdown.hpp"

#include <string>
#include <vector>

namespace tgfmt {

struct Chunk {
    std::string html;    // dikirim dengan parse_mode=HTML
    std::string plain;   // cadangan bila Telegram menolak HTML
};

inline std::string esc(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else o += c;
    }
    return o;
}

inline std::string esc_attr(const std::string& s) {
    std::string o = esc(s);
    std::string r;
    for (char c : o) {
        if (c == '"') r += "&quot;";
        else r += c;
    }
    return r;
}

// Hanya http/https tanpa spasi; skema lain (tg://, javascript:) tidak dijadikan tautan.
inline bool safe_url(const std::string& u) {
    const bool ok = u.rfind("http://", 0) == 0 || u.rfind("https://", 0) == 0;
    return ok && u.find(' ') == std::string::npos;
}

inline std::string safe_lang(const std::string& l) {
    std::string o;
    for (char c : l) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '#' || c == '-' ||
            c == '_' || c == '.') {
            o += c;
        }
        if (o.size() >= 20) break;
    }
    return o;
}

inline std::string plain_of(const std::vector<md::Span>& spans) {
    std::string o;
    for (const auto& sp : spans) o += sp.text;
    return o;
}

inline std::string inline_html(const std::vector<md::Span>& spans) {
    std::string o;
    for (const auto& sp : spans) {
        switch (sp.kind) {
            case md::SpanKind::Bold:   o += "<b>" + esc(sp.text) + "</b>"; break;
            case md::SpanKind::Italic: o += "<i>" + esc(sp.text) + "</i>"; break;
            case md::SpanKind::Code:   o += "<code>" + esc(sp.text) + "</code>"; break;
            case md::SpanKind::Link:
                if (safe_url(sp.url)) {
                    o += "<a href=\"" + esc_attr(sp.url) + "\">" + esc(sp.text) + "</a>";
                } else {
                    o += esc(sp.text) + (sp.url.empty() ? "" : " (" + esc(sp.url) + ")");
                }
                break;
            case md::SpanKind::Text:   o += esc(sp.text); break;
        }
    }
    return o;
}

inline std::string pre_html(const std::string& code, const std::string& lang) {
    const std::string body = esc(code.empty() ? std::string(" ") : code);
    const std::string l = safe_lang(lang);
    if (l.empty()) return "<pre>" + body + "</pre>";
    return "<pre><code class=\"language-" + l + "\">" + body + "</code></pre>";
}

inline std::string block_html(const md::Block& b) {
    const std::string indent(static_cast<std::size_t>(b.level) * 2, ' ');
    switch (b.kind) {
        case md::BlockKind::Heading:  return "<b>" + esc(plain_of(b.spans)) + "</b>";
        case md::BlockKind::Bullet:   return indent + "• " + inline_html(b.spans);
        case md::BlockKind::Numbered: return indent + esc(b.label) + ". " + inline_html(b.spans);
        case md::BlockKind::Quote:    return "▎ <i>" + esc(plain_of(b.spans)) + "</i>";
        case md::BlockKind::Rule:     return "──────────";
        case md::BlockKind::Blank:    return "";
        case md::BlockKind::Code:     return pre_html(b.code, b.label);
        case md::BlockKind::Paragraph: break;
    }
    return inline_html(b.spans);
}

// Batas 4096 Telegram dihitung dari teks tampil, jadi panjang HTML yang
// kita jaga <= max selalu aman.
inline std::vector<Chunk> to_chunks(const std::string& text, std::size_t max = 3500) {
    std::vector<Chunk> out;
    Chunk cur;

    auto flush = [&] {
        if (cur.html.find_first_not_of(" \n") != std::string::npos) out.push_back(cur);
        cur = Chunk{};
    };
    auto add = [&](const std::string& html, const std::string& plain) {
        if (!cur.html.empty() && cur.html.size() + html.size() + 1 > max) flush();
        if (!cur.html.empty()) { cur.html += '\n'; cur.plain += '\n'; }
        cur.html += html;
        cur.plain += plain;
    };

    for (const auto& b : md::parse(text)) {
        const std::string html = block_html(b);
        const std::string plain = (b.kind == md::BlockKind::Code) ? b.code : b.raw;
        if (html.size() <= max) {
            add(html, plain);
        } else if (b.kind == md::BlockKind::Code) {
            // Kode panjang: pecah isinya, tiap potongan jadi blok <pre> sendiri.
            for (const auto& piece : split_message(b.code, max - 300)) {
                add(pre_html(piece, b.label), piece);
            }
        } else {
            // Paragraf raksasa: kirim polos (kehilangan format, tapi tag tidak terbelah).
            for (const auto& piece : split_message(b.raw, max / 2)) add(esc(piece), piece);
        }
    }
    flush();
    if (out.empty()) out.push_back({"(kosong)", "(kosong)"});
    return out;
}

}  // namespace tgfmt