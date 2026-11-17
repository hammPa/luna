#include "tui/tui_render.hpp"

#include "tui/theme.hpp"
#include "tui/tui_text.hpp"

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

Elements line_rows(const Line& l) {
    std::string label = "info";
    Color badge_c = theme::muted;
    Color body_c = theme::muted;
    if (l.role == "user")  { label = "kamu";  badge_c = theme::user_c;  body_c = theme::fg; }
    if (l.role == "luna")  { label = "luna";  badge_c = theme::luna_c;  body_c = theme::fg; }
    if (l.role == "error") { label = "error"; badge_c = theme::error_c; body_c = theme::error_c; }

    Elements rows;
    bool first = true;
    for (const auto& ln : tui_text::split_lines(l.text)) {
        Element left = first ? badge(label, badge_c)
                             : (text("") | size(WIDTH, EQUAL, kBadgeCol));
        Element body = ln.empty() ? text("") : (paragraph(ln) | color(body_c));
        rows.push_back(hbox({std::move(left), std::move(body) | flex}));
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