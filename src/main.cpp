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

#include <cstring>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    const std::string config_path = (argc > 1) ? argv[1] : "config.json";
    const bool telegram_mode = (argc > 2 && std::strcmp(argv[2], "--telegram") == 0);

    // Urutan deklarasi penting: yang dirujuk ChatSession harus
    // dideklarasikan lebih dulu (dihancurkan belakangan).
    Config cfg;
    std::unique_ptr<LLMBackend> backend;
    std::unique_ptr<Memory> memory;
    TelegramPrompter telegram_prompter;
    TerminalPrompter terminal_prompter;
    Prompter& prompter = telegram_mode ? static_cast<Prompter&>(telegram_prompter)
                                   : static_cast<Prompter&>(terminal_prompter);

    std::unique_ptr<PathGuard> guard;
    std::unique_ptr<PermissionGate> gate;
    std::unique_ptr<ToolRegistry> registry;
    std::unique_ptr<ChatSession> session;   // paling akhir = dihancurkan paling dulu

    try {
        cfg = load_config(config_path);
        backend = create_backend(cfg);
        memory = std::make_unique<Memory>(cfg.memory_db);

        gate = std::make_unique<PermissionGate>(make_policy(cfg.permissions), prompter);
        registry = std::make_unique<ToolRegistry>(*gate);
        registry->add(std::make_unique<GetTimeTool>());
        registry->add(std::make_unique<OpenUrlTool>());
        
        if (SwitchWorkspaceTool::available()) {
            registry->add(std::make_unique<SwitchWorkspaceTool>());
        }

        if (!cfg.apps.empty()) {
            registry->add(std::make_unique<OpenAppTool>(cfg.apps));
            
            auto closer = std::make_unique<CloseAppTool>(cfg.apps);
            if (!closer->empty()) registry->add(std::move(closer));
        }
        
        // Fail closed: tanpa allowed_dirs, tool file tidak didaftarkan sama sekali.
        if (!cfg.allowed_dirs.empty()) {
            guard = std::make_unique<PathGuard>(cfg.allowed_dirs);
            registry->add(std::make_unique<ListDirTool>(*guard));
            registry->add(std::make_unique<ReadFileTool>(*guard));
        
            registry->add(std::make_unique<CreateFileTool>(*guard));
            registry->add(std::make_unique<PatchFileTool>(*guard));
            registry->add(std::make_unique<WriteFileTool>(*guard));
            registry->add(std::make_unique<DeleteFileTool>(*guard));
        }

        session = std::make_unique<ChatSession>(cfg, *backend, *memory, *registry);
        session->load();

        std::cerr << "allowed_dirs=" << cfg.allowed_dirs.size()
          << " tools=" << registry->schema().size() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Gagal start: " << e.what() << '\n';
        return 1;
    }

    if (telegram_mode) {
        try {
            run_telegram(cfg, *session, *memory, telegram_prompter);
        } catch (const std::exception& e) {
            std::cerr << "Gagal: " << e.what() << '\n';
            return 1;
        }
        return 0;
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

        if (line[0] == '/' && handle_command(line, *memory, std::cout)) continue;

        try {
            std::cout << session->send(line) << "\n\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
        }
    }

    std::cout << "Sampai jumpa.\n";
    return 0;
}