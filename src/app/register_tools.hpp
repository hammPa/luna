#pragma once

#include "search/search_provider.hpp"
#include "tools/file_sink.hpp"
#include "tools/path_guard.hpp"
#include "tools/tool_registry.hpp"
#include "utils/config.hpp"

#include <memory>

void register_tools(ToolRegistry& registry, const Config& cfg,
                    std::unique_ptr<PathGuard>& guard, FileSink& file_sink,
                    SearchProvider* search, bool telegram_mode);