#pragma once

#include <string>

class ChatSession;
class Memory;
class TuiPrompter;
class ToolRegistry;
class ModelSwitcher;

// Menjalankan layar chat sampai pengguna keluar. Mengembalikan kode exit.
// `models` boleh nullptr; jika ada, header menampilkan model aktif secara langsung
// dan /model tersedia. Jika nullptr, header memakai `status`.
int run_tui(ChatSession& session, Memory& memory, TuiPrompter& prompter,
            const std::string& status, ToolRegistry& tools, int history_max,
            ModelSwitcher* models);