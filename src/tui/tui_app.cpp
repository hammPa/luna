#include "tui/tui_app.hpp"

#include "app/chat_session.hpp"
#include "app/commands.hpp"
#include "memory/memory.hpp"
#include "tui/theme.hpp"
#include "tui/tui_prompter.hpp"
#include "tui/tui_render.hpp"
#include "tui/tui_text.hpp"
#include "tui/slash_menu.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/terminal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

using namespace ftxui;
using tui_text::sanitize;
using tui_text::trim;

namespace {

// Chip tombol di bilah petunjuk: " Enter " kirim
Elements key_hint(const std::string& key, const std::string& desc) {
    return {
        text(" " + key + " ") | bold | color(theme::accent) | bgcolor(theme::input_bg),
        text(" " + desc + "  ") | color(theme::muted),
    };
}

Element hint_bar(int rows_below) {
    if (rows_below > 0) {
        return hbox({
            text(" ↓ " + std::to_string(rows_below) + " baris di bawah ") | bold |
                color(theme::dark) | bgcolor(theme::warn_c),
            text("  PgDn turun · kirim pesan kembali ke bawah") | color(theme::muted),
        });
    }
    Elements h;
    for (auto& part : key_hint("Enter", "kirim")) h.push_back(std::move(part));
    for (auto& part : key_hint("/", "perintah")) h.push_back(std::move(part));
    for (auto& part : key_hint("PgUp/PgDn", "gulir")) h.push_back(std::move(part));
    for (auto& part : key_hint("Ctrl+C", "keluar")) h.push_back(std::move(part));
    return hbox(std::move(h));
}

}  // namespace

int run_tui(ChatSession& session, Memory& memory, TuiPrompter& prompter,
            const std::string& status) {
    // Semua penulis std::cerr (peringatan di mana pun) tidak boleh merusak layar.
    std::ofstream devnull("/dev/null");
    std::streambuf* old_err = std::cerr.rdbuf(devnull.rdbuf());

    auto screen = ScreenInteractive::Fullscreen();

    std::mutex mu;
    std::vector<Line> lines;
    std::string input;
    std::atomic<bool> busy{false};
    std::atomic<bool> quit{false};
    std::atomic<int> frame{0};
    std::thread worker;

    // Hanya disentuh thread UI (event dan render): tanpa mutex.
    int scroll = 0;        // jumlah baris dari bawah; 0 = ikuti pesan terbaru
    int last_total = 0;    // jumlah baris isi pada render sebelumnya

    int menu_sel = 0;              // hanya thread UI
    std::string menu_input;        // untuk mereset pilihan saat input berubah
    int cursor = 0;                // posisi kursor kotak input

    auto refresh = [&] { screen.PostEvent(Event::Custom); };
    auto add = [&](const std::string& role, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu);
        lines.push_back({role, sanitize(msg)});
    };

    prompter.set_notifier(refresh);
    add("info", "Riwayat dimuat: " + std::to_string(session.history_size()) +
                " pesan. Perintah: /remember kunci=nilai, /facts, /clear, exit");

    InputOption input_opt;
    input_opt.cursor_position = &cursor;
    input_opt.transform = [&busy](InputState state) {
        Element e = state.element | color(theme::fg) | bgcolor(theme::input_bg);
        if (state.is_placeholder || busy) e = e | dim;   // placeholder dan saat sibuk lebih redup
        return e;
    };
    auto input_box = Input(&input, "ketik pesan...", input_opt);

    auto view = Renderer(input_box, [&] {
        const Dimensions term = Terminal::Size();

        Elements rows = banner_rows(term.dimx);
        {
            std::lock_guard<std::mutex> lk(mu);
            for (const auto& l : lines) {
                for (auto& r : line_rows(l)) rows.push_back(std::move(r));
            }
        }

        // Posisi gulir dihitung dari baris isi (tanpa spinner).
        const int content = static_cast<int>(rows.size());
        if (scroll > 0 && content > last_total) scroll += content - last_total;
        last_total = content;
        const int view_h = std::max(4, term.dimy - 7);   // kira-kira tinggi area chat
        scroll = std::clamp(scroll, 0, std::max(0, content - view_h));

        if (busy) {
            rows.push_back(hbox({
                text(" luna ") | bold | color(theme::dark) | bgcolor(theme::luna_c),
                text(" "),
                spinner(2, static_cast<size_t>(frame.load())) | color(theme::accent),
                text(" berpikir...") | color(theme::muted),
            }));
        }

        // yframe cenderung menaruh baris fokus di tengah; geser target ke atas
        // setengah tinggi tampilan agar BAWAH tampilan = baris (content-1-scroll).
        size_t target = rows.size() - 1;
        if (scroll > 0) {
            target = static_cast<size_t>(std::max(0, content - 1 - scroll - view_h / 2));
        }
        rows[target] = rows[target] | focus;

        Element chat = vbox(std::move(rows)) | vscroll_indicator | yframe | flex;
        Element header = hbox({
            text(" ✦ Luna ") | bold | color(theme::dark) | bgcolor(theme::accent),
            text("  " + status) | color(theme::muted),
            filler(),
            busy ? (text("● bekerja ") | color(theme::warn_c))
                 : (text("● siap ") | color(theme::luna_c)),
        });

        if (input != menu_input) { menu_input = input; menu_sel = 0; }
        const auto matches = slash_matches(input);
        if (!matches.empty()) {
            menu_sel = std::clamp(menu_sel, 0, static_cast<int>(matches.size()) - 1);
        }

        Elements body;
        body.push_back(header);
        body.push_back(separatorLight() | color(theme::muted));
        body.push_back(chat);
        if (!matches.empty()) body.push_back(slash_menu_view(matches, menu_sel));
        body.push_back(hbox({text(" › ") | bold | color(theme::accent), input_box->Render() | flex})
                           | borderRounded | color(busy ? theme::muted : theme::accent));
        body.push_back(hint_bar(scroll));
        Element main = vbox(std::move(body));

        TuiPrompter::Request req;
        if (prompter.pending(&req)) {
            Element box = vbox({
                text(" Butuh izin ") | bold | color(theme::warn_c),
                separator(),
                paragraphs(sanitize(req.question)),
                separator(),
                text(req.can_always ? "[y] sekali   [a] selalu sesi ini   [n] tolak"
                                    : "[y] sekali   [n] tolak") | bold,
            }) | borderRounded | color(theme::warn_c) | size(WIDTH, LESS_THAN, 72) |
                clear_under | center;
            return dbox({main, box});
        }
        return main;
    });

    auto app = CatchEvent(view, [&](Event e) {
        if (e == Event::Custom) return false;

        // Selama ada pertanyaan izin, semua tombol ditelan.
        TuiPrompter::Request req;
        if (prompter.pending(&req)) {
            if (e == Event::Character("y") || e == Event::Character("Y")) {
                prompter.answer(Decision::AllowOnce);
            } else if (req.can_always &&
                       (e == Event::Character("a") || e == Event::Character("A"))) {
                prompter.answer(Decision::AllowAlways);
            } else if (e == Event::Character("n") || e == Event::Character("N") ||
                       e == Event::Escape) {
                prompter.answer(Decision::Deny);
            }
            return true;
        }

        // Menu perintah "/".
        {
            const auto matches = slash_matches(input);
            if (!matches.empty()) {
                const int n = static_cast<int>(matches.size());
                if (e == Event::ArrowUp)   { menu_sel = (menu_sel + n - 1) % n; return true; }
                if (e == Event::ArrowDown) { menu_sel = (menu_sel + 1) % n; return true; }
                if (e == Event::Escape)    { input.clear(); cursor = 0; return true; }
                if (e == Event::Tab || e == Event::Return) {
                    const SlashCmd& c = matches[static_cast<size_t>(std::clamp(menu_sel, 0, n - 1))];
                    const bool run_now = (e == Event::Return && input == c.name && !c.takes_arg);
                    if (!run_now) {
                        input = c.name;
                        if (c.takes_arg) input += ' ';
                        cursor = static_cast<int>(input.size());
                        return true;
                    }
                    // run_now: lanjut ke bawah dan dijalankan seperti Enter biasa.
                }
            }
        }

        // Gulir riwayat.
        const int page = std::max(3, Terminal::Size().dimy / 2);
        if (e == Event::PageUp)   { scroll += page; return true; }
        if (e == Event::PageDown) { scroll = std::max(0, scroll - page); return true; }
        if (e.is_mouse()) {
            if (e.mouse().button == Mouse::WheelUp)   { scroll += 3; return true; }
            if (e.mouse().button == Mouse::WheelDown) { scroll = std::max(0, scroll - 3); return true; }
            return false;   // klik dll. tetap diteruskan ke kotak input
        }

        if (e != Event::Return) return false;
        if (busy) return true;

        const std::string msg = trim(input);
        input.clear();
        cursor = 0;
        if (msg.empty()) return true;
        if (msg == "exit" || msg == "quit" || msg == "/exit" || msg == "/quit") {
            screen.ExitLoopClosure()();
            return true;
        }

        scroll = 0;   // kirim pesan = kembali ke bawah
        add("user", msg);

        if (msg[0] == '/') {
            std::ostringstream out;
            bool handled = false;
            try {
                handled = handle_command(msg, memory, session, out);
            } catch (const std::exception& ex) {
                out << "Error: " << ex.what();
                handled = true;
            }
            if (handled) {
                add("info", trim(out.str()));
                return true;
            }
        }

        if (worker.joinable()) worker.join();   // giliran sebelumnya sudah selesai
        busy = true;
        worker = std::thread([&, msg] {
            std::string role = "luna";
            std::string reply;
            try {
                reply = session.send(msg);
            } catch (const std::exception& ex) {
                role = "error";
                reply = ex.what();
            } catch (...) {
                role = "error";
                reply = "kesalahan tidak dikenal";
            }
            add(role, reply);
            busy = false;
            refresh();
        });
        return true;
    });

    // Menggerakkan indikator berputar hanya saat sibuk.
    std::thread ticker([&] {
        while (!quit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            if (busy) {
                ++frame;
                refresh();
            }
        }
    });

    screen.Loop(app);

    quit = true;
    prompter.close();                 // izin yang menunggu = ditolak
    if (busy) std::cout << "Menunggu permintaan yang sedang berjalan selesai...\n";
    if (worker.joinable()) worker.join();
    ticker.join();
    prompter.set_notifier(nullptr);

    std::cerr.rdbuf(old_err);
    std::cout << "Sampai jumpa.\n";
    return 0;
}