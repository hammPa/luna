#include "app/chat_session.hpp"
#include "app/commands.hpp"
#include "llm/backend_factory.hpp"
#include "memory/memory.hpp"
#include "utils/config.hpp"

#include "tools/permission.hpp"
#include "tools/prompter.hpp"
#include "tools/tool_registry.hpp"
#include "tools/path_guard.hpp"

#include "tools/builtin_tools.hpp"

#include "telegram/telegram_loop.hpp"
#include "telegram/telegram_prompter.hpp"

#include "telegram/telegram_file_sink.hpp"
#include "search/search_factory.hpp"
#include "utils/log.hpp"

#include "tui/tui_app.hpp"
#include "tui/tui_prompter.hpp"

#include <unistd.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>

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

enum class Mode { Tui, Cli, Telegram };

int main(int argc, char** argv) {
    const std::string config_path = (argc > 1) ? argv[1] : "config.json";

    Mode mode = Mode::Tui;
    if (argc > 2) {
        if (std::strcmp(argv[2], "--telegram") == 0) {
            mode = Mode::Telegram;
        } else if (std::strcmp(argv[2], "--cli") == 0) {
            mode = Mode::Cli;
        } else {
            std::cerr << "Argumen tidak dikenal: " << argv[2]
                      << "\nPakai: luna [config.json] [--cli | --telegram]\n";
            return 1;
        }
    }
    const bool telegram_mode = (mode == Mode::Telegram);

    if (mode == Mode::Tui && (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))) {
        std::cerr << "TUI butuh terminal interaktif. Pakai --cli untuk input dari pipe.\n";
        return 1;
    }

    // Urutan deklarasi penting: yang dirujuk ChatSession harus
    // dideklarasikan lebih dulu (dihancurkan belakangan).
    Config cfg;
    std::unique_ptr<LLMBackend> backend;
    std::unique_ptr<Memory> memory;
    TelegramPrompter telegram_prompter;
    TerminalPrompter terminal_prompter;
    TuiPrompter tui_prompter;
    Prompter& prompter = (mode == Mode::Telegram) ? static_cast<Prompter&>(telegram_prompter)
                       : (mode == Mode::Cli)      ? static_cast<Prompter&>(terminal_prompter)
                                                  : static_cast<Prompter&>(tui_prompter);


    TerminalFileSink terminal_sink;
    TelegramFileSink telegram_sink;
    FileSink& file_sink = telegram_mode ? static_cast<FileSink&>(telegram_sink)
                                        : static_cast<FileSink&>(terminal_sink);

    std::unique_ptr<SearchProvider> search_provider;

    std::unique_ptr<PathGuard> guard;
    std::unique_ptr<PermissionGate> gate;
    std::unique_ptr<ToolRegistry> registry;
    std::unique_ptr<ChatSession> session;   // paling akhir = dihancurkan paling dulu

    try {
        cfg = load_config(config_path);
        logger::init(cfg.log_file, parse_log_level(cfg.log_level),
             static_cast<std::size_t>(cfg.log_max_kb) * 1024);
        
        
        backend = create_backend(cfg);
        memory = std::make_unique<Memory>(cfg.memory_db);
        search_provider = make_search_provider(cfg);

        gate = std::make_unique<PermissionGate>(make_policy(cfg.permissions), prompter);
        registry = std::make_unique<ToolRegistry>(*gate);

        register_tools(*registry, cfg, guard, file_sink, search_provider.get(), telegram_mode);

        session = std::make_unique<ChatSession>(cfg, *backend, *memory, *registry);
        session->load();

        std::string tool_names;
        for (const auto& t : registry->schema()) {
            if (!tool_names.empty()) tool_names += ",";
            tool_names += t.value("name", "?");
        }
        const char* mode_name = telegram_mode ? "telegram" : (mode == Mode::Cli ? "cli" : "tui");
        logger::info("app", std::string("start mode=") + mode_name +
                            " backend=" + backend->name() + " model=" + cfg.model +
                            " history=" + std::to_string(session->history_size()));
        logger::info("app", "tools=" + tool_names);

        std::cerr << "allowed_dirs=" << cfg.allowed_dirs.size()
          << " tools=" << registry->schema().size() << '\n';
    } catch (const std::exception& e) {
        // std::cerr << "Gagal start: " << e.what() << '\n';
        logger::error("app", std::string("Gagal start: ") + e.what());
        return 1;
    }

    if (telegram_mode) {
        try {
            run_telegram(cfg, *session, *memory, telegram_prompter, telegram_sink);
        } catch (const std::exception& e) {
            // std::cerr << "Gagal: " << e.what() << '\n';
            logger::error("app", std::string("Gagal: ") + e.what());
            return 1;
        }
        return 0;
    }

    if (mode == Mode::Tui) {
        const std::string status = backend->name() + " · " + cfg.model + " · " +
                                   std::to_string(registry->schema().size()) + " tool";
        const int rc = run_tui(*session, *memory, tui_prompter, status);
        logger::info("app", "berhenti");
        return rc;
    }

    std::cout << "Backend: " << backend->name() << " | Model: " << cfg.model
              << " | Riwayat dimuat: " << session->history_size() << " pesan"
              << "\nPerintah: /remember kunci=nilai, /facts, exit\n";

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        if (line[0] == '/' && handle_command(line, *memory, *session, std::cout)) continue;

        try {
            std::cout << session->send(line) << "\n\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
        }
    }
    
    logger::info("app", "berhenti");
    std::cout << "Sampai jumpa.\n";
    return 0;
}