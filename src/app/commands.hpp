#pragma once

#include "memory/memory.hpp"
#include "app/chat_session.hpp"

#include <iosfwd>
#include <string>

class ToolRegistry;
class ModelSwitcher;

// Mengembalikan true jika `line` adalah perintah slash dan sudah ditangani.
// Output ditulis ke `out` (bukan langsung cout) supaya mudah dialihkan nanti.
// `tools` dan `models` opsional: nullptr (Telegram) = perintah /tools, /tool,
// /profile, dan /model tidak dikenali.

bool handle_command(const std::string& line, Memory& memory, ChatSession& session,
                    std::ostream& out, ToolRegistry* tools = nullptr,
                    ModelSwitcher* models = nullptr);