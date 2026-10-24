#pragma once

#include "memory/memory.hpp"
#include "app/chat_session.hpp"

#include <iosfwd>
#include <string>

// Mengembalikan true jika `line` adalah perintah slash dan sudah ditangani.
// Output ditulis ke `out` (bukan langsung cout) supaya mudah dialihkan nanti.

bool handle_command(const std::string& line, Memory& memory, ChatSession& session,
                    std::ostream& out);