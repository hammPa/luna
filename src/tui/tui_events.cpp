#include "tui/tui_events.hpp"

#include "app/commands.hpp"
#include "tui/slash_menu.hpp"
#include "tui/tui_text.hpp"

#include "ftxui/screen/terminal.hpp"
#include "tui/clipboard.hpp"
#include "utils/cancel.hpp"
#include "utils/markdown.hpp"

#include <algorithm>
#include <exception>
#include <sstream>

using namespace ftxui;
using tui_text::trim;

namespace {

// Selama ada pertanyaan izin, semua tombol ditelan.
bool handle_permission(const Event& e, TuiDeps& d) {
    TuiPrompter::Request req;
    if (!d.prompter.pending(&req)) return false;

    if (e == Event::Character("y") || e == Event::Character("Y")) {
        d.prompter.answer(Decision::AllowOnce);
    } else if (req.can_always &&
               (e == Event::Character("a") || e == Event::Character("A"))) {
        d.prompter.answer(Decision::AllowAlways);
    } else if (e == Event::Character("n") || e == Event::Character("N") ||
               e == Event::Escape) {
        d.prompter.answer(Decision::Deny);
    }
    return true;
}

// Menu perintah "/". false = tidak ditangani (atau lanjut menjalankan perintah).
bool handle_menu(const Event& e, TuiState& st) {
    const auto matches = slash_matches(st.input);
    if (matches.empty()) return false;

    const int n = static_cast<int>(matches.size());
    if (e == Event::ArrowUp)   { st.menu_sel = (st.menu_sel + n - 1) % n; return true; }
    if (e == Event::ArrowDown) { st.menu_sel = (st.menu_sel + 1) % n; return true; }
    if (e == Event::Escape)    { st.input.clear(); st.cursor = 0; return true; }
    if (e == Event::Tab || e == Event::Return) {
        const SlashCmd& c = matches[static_cast<size_t>(std::clamp(st.menu_sel, 0, n - 1))];
        const bool run_now = (e == Event::Return && st.input == c.name && !c.takes_arg);
        if (!run_now) {
            st.input = c.name;
            if (c.takes_arg) st.input += ' ';
            st.cursor = static_cast<int>(st.input.size());
            return true;
        }
        // run_now: lanjut, dijalankan seperti Enter biasa.
    }
    return false;
}

bool handle_scroll(const Event& ev, TuiState& st) {
    Event e = ev;   // mouse() tidak const di FTXUI 6.1.9
    const int page = std::max(3, Terminal::Size().dimy / 2);
    if (e == Event::PageUp)   { st.scroll += page; return true; }
    if (e == Event::PageDown) { st.scroll = std::max(0, st.scroll - page); return true; }
    if (e.is_mouse()) {
        if (e.mouse().button == Mouse::WheelUp)   { st.scroll += 3; return true; }
        if (e.mouse().button == Mouse::WheelDown) { st.scroll = std::max(0, st.scroll - 3); return true; }
    }
    return false;   // klik dll. tetap diteruskan ke kotak input
}

// Esc saat sibuk = batalkan giliran. (Kotak izin sudah ditangani lebih dulu.)
bool handle_cancel(const Event& e, TuiDeps& d) {
    if (!d.st.busy || e != Event::Escape) return false;
    if (!d.st.cancelling.exchange(true)) {
        d.session.cancel();
        d.st.add("info", "Membatalkan giliran...");
    }
    return true;
}

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

bool handle_submit(const Event& e, TuiDeps& d) {
    if (e != Event::Return) return false;
    TuiState& st = d.st;
    if (st.busy) return true;

    const std::string msg = trim(st.input);
    st.input.clear();
    st.cursor = 0;
    if (msg.empty()) return true;
    if (msg == "exit" || msg == "quit" || msg == "/exit" || msg == "/quit") {
        d.exit_loop();
        return true;
    }

    st.scroll = 0;   // kirim pesan = kembali ke bawah
    st.add("user", msg);
    
    if (msg == "/copy" || msg == "/copy code") {
        do_copy(st, msg == "/copy code");
        return true;
    }
    if (msg[0] == '/') {
        std::ostringstream out;
        bool handled = false;
        try {
            handled = handle_command(msg, d.memory, d.session, out);
        } catch (const std::exception& ex) {
            out << "Error: " << ex.what();
            handled = true;
        }
        if (handled) {
            st.add("info", trim(out.str()));
            return true;
        }
    }

    if (d.worker.joinable()) d.worker.join();   // giliran sebelumnya sudah selesai
    st.busy = true;
    d.worker = std::thread([&d, msg] {
        std::string role = "luna";
        std::string reply;
                try {
            reply = d.session.send(msg);
        } catch (const CancelledError&) {
            role = "info";
            reply = "Giliran dibatalkan.";
        } catch (const std::exception& ex) {
            role = "error";
            reply = ex.what();
        } catch (...) {
            role = "error";
            reply = "kesalahan tidak dikenal";
        }
        d.st.add(role, reply);
        d.st.cancelling = false;
        d.st.busy = false;
        d.refresh();
    });
    return true;
}

}  // namespace

bool handle_event(const Event& e, TuiDeps& d) {
    if (e == Event::Custom) return false;
    if (handle_permission(e, d)) return true;
    if (handle_cancel(e, d)) return true;
    if (handle_menu(e, d.st)) return true;
    if (handle_scroll(e, d.st)) return true;
    return handle_submit(e, d);
}