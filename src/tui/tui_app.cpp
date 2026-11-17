#include "tui/tui_app.hpp"

#include "app/chat_session.hpp"
#include "app/commands.hpp"
#include "memory/memory.hpp"
#include "tui/tui_prompter.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

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

namespace {

struct Line {
    std::string role;   // user | luna | info | error
    std::string text;
};

// Teks dari model atau web bisa berisi escape terminal. Buang karakter kontrol
// kecuali newline agar tidak merusak atau memanipulasi layar.
std::string sanitize(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        if (c == '\n') out += '\n';
        else if (c == '\t') out += ' ';
        else if (c < 0x20 || c == 0x7f) continue;
        else out += static_cast<char>(c);
    }
    return out;
}

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '\n') { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

Element paragraphs(const std::string& content) {
    Elements rows;
    for (const auto& ln : split_lines(content)) {
        rows.push_back(ln.empty() ? text("") : paragraph(ln));
    }
    return vbox(std::move(rows));
}

Element render_line(const Line& l) {
    std::string label = "info";
    Color c = Color::GrayDark;
    if (l.role == "user")  { label = "kamu"; c = Color::Cyan; }
    if (l.role == "luna")  { label = "luna"; c = Color::Green; }
    if (l.role == "error") { label = "error"; c = Color::Red; }
    return hbox({
        text(label) | bold | color(c) | size(WIDTH, EQUAL, 6),
        paragraphs(l.text) | flex,
    });
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

    auto refresh = [&] { screen.PostEvent(Event::Custom); };
    auto add = [&](const std::string& role, const std::string& text) {
        std::lock_guard<std::mutex> lk(mu);
        lines.push_back({role, sanitize(text)});
    };

    prompter.set_notifier(refresh);
    add("info", "Riwayat dimuat: " + std::to_string(session.history_size()) +
                " pesan. Perintah: /remember kunci=nilai, /facts, /clear, exit");

    auto input_box = Input(&input, "ketik pesan...");

    auto view = Renderer(input_box, [&] {
        Elements rows;
        {
            std::lock_guard<std::mutex> lk(mu);
            for (const auto& l : lines) rows.push_back(render_line(l));
        }
        if (busy) {
            rows.push_back(hbox({
                text("luna  ") | bold | color(Color::Green),
                spinner(2, static_cast<size_t>(frame.load())),
                text(" berpikir..."),
            }));
        }
        if (!rows.empty()) rows.back() = rows.back() | focus;   // ikuti pesan terbaru

        Element chat = vbox(std::move(rows)) | vscroll_indicator | yframe | flex;
        Element main = vbox({
            hbox({text(" Luna ") | bold | inverted, text(" " + status) | dim, filler()}),
            separator(),
            chat,
            hbox({text("> ") | bold, input_box->Render() | flex}) | border,
            text(" Enter kirim · exit atau Ctrl+C keluar") | dim,
        });

        TuiPrompter::Request req;
        if (prompter.pending(&req)) {
            Element box = vbox({
                text(" Butuh izin ") | bold | color(Color::Yellow),
                separator(),
                paragraphs(sanitize(req.question)),
                separator(),
                text(req.can_always ? "[y] sekali   [a] selalu sesi ini   [n] tolak"
                                    : "[y] sekali   [n] tolak") | bold,
            }) | border | size(WIDTH, LESS_THAN, 72) | clear_under | center;
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

        if (e != Event::Return) return false;
        if (busy) return true;

        const std::string text = trim(input);
        input.clear();
        if (text.empty()) return true;
        if (text == "exit" || text == "quit") {
            screen.ExitLoopClosure()();
            return true;
        }

        add("user", text);

        if (text[0] == '/') {
            std::ostringstream out;
            bool handled = false;
            try {
                handled = handle_command(text, memory, session, out);
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
        worker = std::thread([&, text] {
            std::string role = "luna";
            std::string reply;
            try {
                reply = session.send(text);
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