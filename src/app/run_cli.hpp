#pragma once

#include "app/chat_session.hpp"
#include "llm/llm_backend.hpp"
#include "memory/memory.hpp"
#include "utils/config.hpp"
#include "tools/tool_registry.hpp"

// Menjalankan mode CLI (stdin/stdout). Mengembalikan kode keluar proses.
int run_cli(const LLMBackend& backend, const Config& cfg,
            ChatSession& session, Memory& memory, ToolRegistry& tools);