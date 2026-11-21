#pragma once

#include "app/chat_session.hpp"
#include "memory/memory.hpp"
#include "tui/tui_prompter.hpp"
#include "tui/tui_state.hpp"

#include "ftxui/component/event.hpp"

#include <functional>
#include <thread>

class ToolRegistry;
class ModelSwitcher;

struct TuiDeps {
    ChatSession& session;
    Memory& memory;
    TuiPrompter& prompter;
    TuiState& st;
    std::function<void()> refresh;     // minta render ulang
    std::function<void()> exit_loop;   // keluar dari screen.Loop
    std::thread& worker;               // thread giliran chat
    ToolRegistry& tools;
    ModelSwitcher* models;             // nullptr = /model tidak tersedia
};

// true = event sudah ditangani (tidak diteruskan ke kotak input).
bool handle_event(const ftxui::Event& e, TuiDeps& d);