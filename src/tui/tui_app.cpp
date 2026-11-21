#include "tui/tui_app.hpp"

#include "app/chat_session.hpp"
#include "app/model_switcher.hpp"
#include "memory/memory.hpp"
#include "tui/theme.hpp"
#include "tui/tui_events.hpp"
#include "tui/tui_prompter.hpp"
#include "tui/tui_state.hpp"
#include "tui/tui_view.hpp"
#include "tools/tool_registry.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace ftxui;

int run_tui(ChatSession& session, Memory& memory, TuiPrompter& prompter,
            const std::string& status, ToolRegistry& tools, int history_max,
            ModelSwitcher* models) {
    // Semua penulis std::cerr (peringatan di mana pun) tidak boleh merusak layar.
    std::ofstream devnull("/dev/null");
    std::streambuf* old_err = std::cerr.rdbuf(devnull.rdbuf());

    auto screen = ScreenInteractive::Fullscreen();

    TuiState st;
    st.history_max = static_cast<std::size_t>(history_max);
    std::thread worker;

    auto refresh = [&] { screen.PostEvent(Event::Custom); };

    prompter.set_notifier(refresh);
    st.add("info", "Riwayat dimuat: " + std::to_string(session.history_size()) +
                   " pesan. Perintah: /remember kunci=nilai, /facts, /clear, exit");

    InputOption input_opt;
    input_opt.cursor_position = &st.cursor;
    input_opt.transform = [&st](InputState state) {
        Element e = state.element | color(theme::fg) | bgcolor(theme::input_bg);
        if (state.is_placeholder || st.busy) e = e | dim;   // placeholder dan saat sibuk lebih redup
        return e;
    };
    auto input_box = Input(&st.input, "ketik pesan...", input_opt);

    auto view = Renderer(input_box, [&] {
        std::size_t on = 0;
        for (const auto& t : tools.list()) if (t.enabled) ++on;
        std::string head = models ? models->label() : status;
        head += " · " + std::to_string(on) + " tool";
        if (!tools.active_profile().empty()) head += " · profil " + tools.active_profile();
        return render_screen(st, session, prompter, head, input_box->Render());
    });

    TuiDeps deps{session, memory, prompter, st, refresh,
                 [&] { screen.ExitLoopClosure()(); }, worker, tools, models};
    auto app = CatchEvent(view, [&](Event e) { return handle_event(e, deps); });

    // Menggerakkan indikator berputar hanya saat sibuk.
    std::thread ticker([&] {
        while (!st.quit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            if (st.busy) {
                ++st.frame;
                refresh();
            }
        }
    });

    screen.Loop(app);

    st.quit = true;
    prompter.close();                 // izin yang menunggu = ditolak
    if (st.busy) std::cout << "Menunggu permintaan yang sedang berjalan selesai...\n";
    if (worker.joinable()) worker.join();
    ticker.join();
    prompter.set_notifier(nullptr);

    std::cerr.rdbuf(old_err);
    std::cout << "Sampai jumpa.\n";
    return 0;
}