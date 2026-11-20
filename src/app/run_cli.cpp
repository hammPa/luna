#include "app/run_cli.hpp"

#include "app/commands.hpp"
#include "utils/cancel.hpp"
#include "utils/ctx_format.hpp"
#include "utils/log.hpp"
#include "utils/markdown_ansi.hpp"

#include <unistd.h>

#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <string>

namespace {

std::atomic<bool>* g_cancel = nullptr;
volatile std::sig_atomic_t g_busy = 0;

// Ctrl+C: saat menunggu jawaban = batalkan giliran. Saat menunggu input,
// getline terputus (tanpa SA_RESTART) dan loop keluar rapi.
void on_sigint(int) {
    if (g_busy && g_cancel) g_cancel->store(true);
}

}  // namespace

int run_cli(const LLMBackend& backend, const Config& cfg,
            ChatSession& session, Memory& memory) {
    std::cout << "Backend: " << backend.name() << " | Model: " << cfg.model
              << " | Riwayat dimuat: " << session.history_size() << " pesan"
              << "\nPerintah: /remember kunci=nilai, /facts, exit\n";

    g_cancel = session.cancel_flag();
    struct sigaction sa {};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                  // tanpa SA_RESTART
    sigaction(SIGINT, &sa, nullptr);

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        if (line[0] == '/' && handle_command(line, memory, session, std::cout)) continue;

        g_busy = 1;
        try {
            // std::cout << session.send(line) << "\n\n";
            const std::string reply = session.send(line);
            std::cout << (isatty(STDOUT_FILENO) ? md::to_ansi(reply) : reply) << "\n";
            std::cout << "\x1b[2m[" << ctx_fmt::summary(session.ctx_used(), session.ctx_max(),
                                                        session.ctx_estimated())
                      << "]\x1b[0m\n\n";
        } catch (const CancelledError&) {
            std::cout << "Dibatalkan.\n\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
        }
        g_busy = 0;
    }

    logger::info("app", "berhenti");
    std::cout << "Sampai jumpa.\n";
    return 0;
}