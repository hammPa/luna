#pragma once

#include "app/chat_session.hpp"
#include "memory/memory.hpp"
#include "utils/config.hpp"

// Jalan sampai proses dihentikan (Ctrl+C). Lempar exception hanya untuk
// kesalahan konfigurasi saat start.
void run_telegram(const Config& cfg, ChatSession& session, Memory& memory);