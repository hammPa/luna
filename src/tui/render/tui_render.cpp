#include "tui/render/tui_render.hpp"
#include "tui/render/theme.hpp"
#include "tui/render/tui_text.hpp"
#include "tui/render/tui_markdown.hpp"
#include "utils/ctx_format.hpp"

using namespace ftxui;

namespace {

constexpr int kBadgeCol = 9;   // lebar kolom lencana

Element badge(const std::string& label, Color c) {
    return hbox({
               text(" " + label + " ") | bold | color(theme::dark) | bgcolor(c),
               filler(),
           }) | size(WIDTH, EQUAL, kBadgeCol);
}

}  // namespace

Elements banner_rows(int width) {
    Elements rows;
    if (width < 40) {
        rows.push_back(text(" ✦ LUNA") | bold | color(theme::accent));
        rows.push_back(text(" asisten AI pribadi") | color(theme::muted));
        rows.push_back(text(""));
        return rows;
    }
    static const char* art[6] = {
        "██╗     ██╗   ██╗███╗   ██╗ █████╗ ",
        "██║     ██║   ██║████╗  ██║██╔══██╗",
        "██║     ██║   ██║██╔██╗ ██║███████║",
        "██║     ██║   ██║██║╚██╗██║██╔══██║",
        "███████╗╚██████╔╝██║ ╚████║██║  ██║",
        "╚══════╝ ╚═════╝ ╚═╝  ╚═══╝╚═╝  ╚═╝",
    };
    for (int i = 0; i < 6; ++i) {
        rows.push_back(text(art[i]) | bold | color(theme::banner[i]));
    }
    rows.push_back(text("  asisten AI pribadi") | color(theme::muted));
    rows.push_back(text(""));
    return rows;
}

Element paragraphs(const std::string& content) {
    Elements rows;
    for (const auto& ln : tui_text::split_lines(content)) {
        rows.push_back(ln.empty() ? text("") : paragraph(ln));
    }
    return vbox(std::move(rows));
}

Elements line_rows(const Line& l, int width) {
    std::string label = "info";
    Color badge_c = theme::muted;
    Color body_c = theme::muted;
    if (l.role == "user")  { label = "kamu";  badge_c = theme::user_c;  body_c = theme::fg; }
    if (l.role == "luna")  { label = "luna";  badge_c = theme::luna_c;  body_c = theme::fg; }
    if (l.role == "error") { label = "error"; badge_c = theme::error_c; body_c = theme::error_c; }

    const int avail = std::max(10, width - kBadgeCol - 1);

    // Hanya balasan Luna yang dirender sebagai markdown. Pesan lain polos,
    // tapi tetap dibungkus per baris layar agar hitungan gulir akurat.
    Elements body;
    if (l.role == "luna") {
        body = markdown_rows(l.text, body_c, avail);
    } else {
        for (const auto& ln : tui_text::split_lines(l.text)) {
            for (auto& r : markdown_rows(tui_text::sanitize(ln), body_c, avail)) {
                body.push_back(std::move(r));
            }
        }
    }

    Elements rows;
    bool first = true;
    for (auto& b : body) {
        Element left = first ? badge(label, badge_c)
                             : (text("") | size(WIDTH, EQUAL, kBadgeCol));
        rows.push_back(hbox({std::move(left), std::move(b) | flex}));
        first = false;
    }
    rows.push_back(text(""));   // jarak antar pesan
    return rows;
}

Element slash_menu_view(const std::vector<SlashCmd>& items, int selected) {
    Elements rows;
    for (size_t i = 0; i < items.size(); ++i) {
        const bool sel = static_cast<int>(i) == selected;
        Element row = hbox({
            text(" " + items[i].name) | bold | color(sel ? theme::accent : theme::fg) |
                size(WIDTH, EQUAL, 12),
            text(items[i].desc + " ") | color(theme::muted),
        });
        if (sel) row = row | bgcolor(theme::input_bg);
        rows.push_back(std::move(row));
    }
    return vbox(std::move(rows)) | borderRounded | color(theme::muted);
}

namespace {

// Chip tombol di bilah petunjuk: " Enter " kirim
Elements key_hint(const std::string& key, const std::string& desc) {
    return {
        text(" " + key + " ") | bold | color(theme::accent) | bgcolor(theme::input_bg),
        text(" " + desc + "  ") | color(theme::muted),
    };
}

}  // namespace

Element hint_bar(int rows_below, bool busy) {
    if (rows_below > 0) {
        return hbox({
            text(" ↓ " + std::to_string(rows_below) + " baris di bawah ") | bold |
                color(theme::dark) | bgcolor(theme::warn_c),
            text("  PgDn turun · kirim pesan kembali ke bawah") | color(theme::muted),
        });
    }
    if (busy) {
        Elements h;
        for (auto& part : key_hint("Esc", "batalkan")) h.push_back(std::move(part));
        for (auto& part : key_hint("PgUp/PgDn", "gulir")) h.push_back(std::move(part));
        return hbox(std::move(h));
    }
    Elements h;
    for (auto& part : key_hint("Enter", "kirim")) h.push_back(std::move(part));
    for (auto& part : key_hint("/", "perintah")) h.push_back(std::move(part));
    for (auto& part : key_hint("PgUp/PgDn", "gulir")) h.push_back(std::move(part));
    for (auto& part : key_hint("Ctrl+C", "keluar")) h.push_back(std::move(part));
    return hbox(std::move(h));
}

Element header_bar(const std::string& status, int used, int max, bool estimated, bool busy) {
    const int pct = ctx_fmt::percent(used, max);
    const Color ctx_c = (pct < 0) ? theme::muted
                        : (ctx_fmt::level(pct) == 0 ? theme::luna_c
                        : ctx_fmt::level(pct) == 1 ? theme::warn_c : theme::error_c);
    return hbox({
        text(" ✦ Luna ") | bold | color(theme::dark) | bgcolor(theme::accent),
        text("  " + status) | color(theme::muted),
        filler(),
        text(ctx_fmt::label(used, max, estimated) + " ") | color(ctx_c),
        text(ctx_fmt::bar(pct) + "  ") | color(ctx_c),
        busy ? (text("● bekerja ") | color(theme::warn_c))
             : (text("● siap ") | color(theme::luna_c)),
    });
}

Element busy_row(int frame, const std::string& notice) {
    return hbox({
        text(" luna ") | bold | color(theme::dark) | bgcolor(theme::luna_c),
        text(" "),
        spinner(2, static_cast<size_t>(frame)) | color(theme::accent),
        text(notice.empty() ? " berpikir..." : " " + notice) |
            color(notice.empty() ? theme::muted : theme::warn_c),
    });
}

Element input_bar(Element input_el, bool busy) {
    return hbox({text(" › ") | bold | color(theme::accent), std::move(input_el) | flex})
           | borderRounded | color(busy ? theme::muted : theme::accent);
}

Element permission_box(const std::string& question, bool can_always) {
    return vbox({
               text(" Butuh izin ") | bold | color(theme::warn_c),
               separator(),
               paragraphs(tui_text::sanitize(question)),
               separator(),
               text(can_always ? "[y] sekali   [a] selalu sesi ini   [n] tolak"
                               : "[y] sekali   [n] tolak") | bold,
           }) | borderRounded | color(theme::warn_c) | size(WIDTH, LESS_THAN, 72) |
           clear_under | center;
}