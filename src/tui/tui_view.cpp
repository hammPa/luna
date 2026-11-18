#include "tui/tui_view.hpp"

#include "app/chat_session.hpp"
#include "tui/slash_menu.hpp"
#include "tui/theme.hpp"
#include "tui/tui_prompter.hpp"
#include "tui/tui_render.hpp"
#include "tui/tui_state.hpp"

#include "ftxui/screen/terminal.hpp"

#include <algorithm>

using namespace ftxui;

Element render_screen(TuiState& st, ChatSession& session, TuiPrompter& prompter,
                      const std::string& status, Element input_el) {
    const Dimensions term = Terminal::Size();

    Elements rows = banner_rows(term.dimx);
    {
        std::lock_guard<std::mutex> lk(st.mu);
        for (const auto& l : st.lines) {
            for (auto& r : line_rows(l, term.dimx)) rows.push_back(std::move(r));
        }
    }

    // Posisi gulir dihitung dari baris isi (tanpa spinner).
    const int content = static_cast<int>(rows.size());
    if (st.scroll > 0 && content > st.last_total) st.scroll += content - st.last_total;
    st.last_total = content;
    const int view_h = std::max(4, term.dimy - 7);   // kira-kira tinggi area chat
    st.scroll = std::clamp(st.scroll, 0, std::max(0, content - view_h));

    if (st.busy) rows.push_back(busy_row(st.frame.load()));

    // yframe cenderung menaruh baris fokus di tengah; geser target ke atas
    // setengah tinggi tampilan agar BAWAH tampilan = baris (content-1-scroll).
    size_t target = rows.size() - 1;
    if (st.scroll > 0) {
        target = static_cast<size_t>(std::max(0, content - 1 - st.scroll - view_h / 2));
    }
    rows[target] = rows[target] | focus;

    Element chat = vbox(std::move(rows)) | vscroll_indicator | yframe | flex;
    Element header = header_bar(status, session.ctx_used(), session.ctx_max(),
                                session.ctx_estimated(), st.busy);

    if (st.input != st.menu_input) { st.menu_input = st.input; st.menu_sel = 0; }
    const auto matches = slash_matches(st.input);
    if (!matches.empty()) {
        st.menu_sel = std::clamp(st.menu_sel, 0, static_cast<int>(matches.size()) - 1);
    }

    Elements body;
    body.push_back(header);
    body.push_back(separatorLight() | color(theme::muted));
    body.push_back(chat);
    if (!matches.empty()) body.push_back(slash_menu_view(matches, st.menu_sel));
    body.push_back(input_bar(std::move(input_el), st.busy));
    body.push_back(hint_bar(st.scroll, st.busy));
    Element main = vbox(std::move(body));

    TuiPrompter::Request req;
    if (prompter.pending(&req)) {
        return dbox({main, permission_box(req.question, req.can_always)});
    }
    return main;
}