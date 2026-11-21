#include "tui/input/input_history.hpp"

#include <cstddef>

using namespace ftxui;

void push_history(TuiState& st, const std::string& msg) {
    st.hist_pos = -1;
    st.draft.clear();
    if (st.history_max == 0) return;
    if (!st.input_history.empty() && st.input_history.back() == msg) return;
    st.input_history.push_back(msg);
    while (st.input_history.size() > st.history_max) {
        st.input_history.erase(st.input_history.begin());
    }
}

bool handle_history(const Event& e, TuiState& st) {
    if (e != Event::ArrowUp && e != Event::ArrowDown) return false;
    if (st.input_history.empty()) return false;
    const int n = static_cast<int>(st.input_history.size());

    if (e == Event::ArrowUp) {
        if (st.hist_pos == -1) {
            st.draft = st.input;
            st.hist_pos = n - 1;
        } else if (st.hist_pos > 0) {
            --st.hist_pos;
        }
    } else {
        if (st.hist_pos == -1) return true;
        if (st.hist_pos < n - 1) ++st.hist_pos;
        else st.hist_pos = -1;
    }
    st.input = (st.hist_pos == -1) ? st.draft
                                   : st.input_history[static_cast<size_t>(st.hist_pos)];
    st.cursor = static_cast<int>(st.input.size());
    return true;
}