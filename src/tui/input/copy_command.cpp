#include "tui/input/copy_command.hpp"

#include "tui/input/clipboard.hpp"
#include "utils/markdown.hpp"

#include <mutex>
#include <string>

void do_copy(TuiState& st, bool code_only) {
    std::string reply;
    {
        std::lock_guard<std::mutex> lk(st.mu);
        for (auto it = st.lines.rbegin(); it != st.lines.rend(); ++it) {
            if (it->role == "luna") { reply = it->text; break; }
        }
    }
    if (reply.empty()) {
        st.add("info", "Belum ada balasan Luna untuk disalin.");
        return;
    }
    std::string payload = reply;
    if (code_only) {
        payload.clear();
        for (const auto& b : md::parse(reply)) {
            if (b.kind == md::BlockKind::Code) payload = b.code;   // yang terakhir
        }
        if (payload.empty()) {
            st.add("info", "Balasan terakhir tidak punya blok kode.");
            return;
        }
    }
    if (clipboard::copy(payload)) {
        st.add("info", "Tersalin " + std::to_string(payload.size()) + " karakter ke clipboard.");
    } else {
        st.add("info", "Gagal menyalin (kosong atau lebih dari 100000 karakter).");
    }
}