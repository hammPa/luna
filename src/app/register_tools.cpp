#include "app/register_tools.hpp"

#include "tools/builtin_tools.hpp"

#include <cstdint>
#include <memory>

namespace {

void register_basic_tools(ToolRegistry& registry) {
    registry.add(std::make_unique<GetTimeTool>());
    registry.add(std::make_unique<OpenUrlTool>());
}

void register_window_tools(ToolRegistry& registry, bool telegram_mode) {
    if (!SwitchWorkspaceTool::available()) return;   // bukan Hyprland
    registry.add(std::make_unique<SwitchWorkspaceTool>());
    registry.add(std::make_unique<ListWindowsTool>());
    registry.add(std::make_unique<FocusWindowTool>());
    registry.add(std::make_unique<CloseWindowTool>(!telegram_mode));
}

void register_app_tools(ToolRegistry& registry, const Config& cfg) {
    if (cfg.apps.empty()) return;
    registry.add(std::make_unique<OpenAppTool>(cfg.apps));

    auto closer = std::make_unique<CloseAppTool>(cfg.apps);
    if (!closer->empty()) registry.add(std::move(closer));
}

// Fail closed: tanpa allowed_dirs, tool file tidak didaftarkan sama sekali.
void register_file_tools(ToolRegistry& registry, const Config& cfg,
                         std::unique_ptr<PathGuard>& guard, FileSink& file_sink) {
    if (cfg.allowed_dirs.empty()) return;
    guard = std::make_unique<PathGuard>(cfg.allowed_dirs);
    registry.add(std::make_unique<ListDirTool>(*guard));
    registry.add(std::make_unique<ReadFileTool>(*guard));
    registry.add(std::make_unique<CreateFileTool>(*guard));
    registry.add(std::make_unique<PatchFileTool>(*guard));
    registry.add(std::make_unique<WriteFileTool>(*guard));
    registry.add(std::make_unique<DeleteFileTool>(*guard));
    registry.add(std::make_unique<SearchFilesTool>(*guard));
    registry.add(std::make_unique<SendFileTool>(
        *guard, file_sink, static_cast<std::uintmax_t>(cfg.max_send_mb) * 1024 * 1024));
}

void register_web_tools(ToolRegistry& registry, const Config& cfg, SearchProvider* search) {
    registry.add(std::make_unique<WeatherTool>(cfg.default_location, 15L));
    if (search) {
        registry.add(std::make_unique<SearchWebTool>(*search, cfg.search_max_results));
    }
}

}  // namespace

void register_tools(ToolRegistry& registry, const Config& cfg,
                    std::unique_ptr<PathGuard>& guard, FileSink& file_sink,
                    SearchProvider* search, bool telegram_mode) {
    register_basic_tools(registry);
    register_window_tools(registry, telegram_mode);
    register_app_tools(registry, cfg);
    register_file_tools(registry, cfg, guard, file_sink);
    register_web_tools(registry, cfg, search);
}