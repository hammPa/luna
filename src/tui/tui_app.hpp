#pragma once

#include <string>

class ChatSession;
class Memory;
class TuiPrompter;
class ToolRegistry;

// Menjalankan layar chat sampai pengguna keluar. Mengembalikan kode exit.
int run_tui(ChatSession& session, Memory& memory, TuiPrompter& prompter,
            const std::string& status, ToolRegistry& tools);