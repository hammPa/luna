#pragma once

#include "app/chat_session.hpp"
#include "app/model_switcher.hpp"
#include "llm/llm_backend.hpp"
#include "memory/memory.hpp"
#include "tools/tool_registry.hpp"
#include "utils/config.hpp"

// Menjalankan mode CLI (stdin/stdout). Mengembalikan kode keluar proses.
int run_cli(const LLMBackend& backend, const Config& cfg,
            ChatSession& session, Memory& memory, ToolRegistry& tools,
            ModelSwitcher& models);