#include "app/chat_session.hpp"
#include "app/commands.hpp"
#include "llm/backend_factory.hpp"
#include "memory/memory.hpp"
#include "utils/config.hpp"

#include "tools/builtin/get_time.hpp"
#include "tools/permission.hpp"
#include "tools/prompter.hpp"
#include "tools/tool_registry.hpp"

#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    const std::string config_path = (argc > 1) ? argv[1] : "config.json";

    // Urutan deklarasi penting: yang dirujuk ChatSession harus
    // dideklarasikan lebih dulu (dihancurkan belakangan).
    Config cfg;
    std::unique_ptr<LLMBackend> backend;
    std::unique_ptr<Memory> memory;
    TerminalPrompter prompter;
    std::unique_ptr<PermissionGate> gate;
    std::unique_ptr<ToolRegistry> registry;
    std::unique_ptr<ChatSession> session;   // paling akhir = dihancurkan paling dulu

    try {
        cfg = load_config(config_path);
        backend = create_backend(cfg);
        memory = std::make_unique<Memory>(cfg.memory_db);

        gate = std::make_unique<PermissionGate>(PermissionPolicy{}, prompter);
        registry = std::make_unique<ToolRegistry>(*gate);
        registry->add(std::make_unique<GetTimeTool>());

        session = std::make_unique<ChatSession>(cfg, *backend, *memory, *registry);
        session->load();
    } catch (const std::exception& e) {
        std::cerr << "Gagal start: " << e.what() << '\n';
        return 1;
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