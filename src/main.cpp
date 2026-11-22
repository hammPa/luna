#include "app/chat_session.hpp"
#include "app/model_switcher.hpp"
#include "cli/run_cli.hpp"
#include "llm/backend_factory.hpp"
#include "memory/memory.hpp"
#include "utils/config.hpp"

#include "tools/permission.hpp"
#include "tools/prompter.hpp"
#include "tools/tool_registry.hpp"
#include "tools/path_guard.hpp"

#include "telegram/telegram_loop.hpp"
#include "telegram/telegram_prompter.hpp"

#include "telegram/telegram_file_sink.hpp"
#include "search/search_factory.hpp"
#include "utils/log.hpp"

#include "tui/tui_app.hpp"
#include "tui/tui_prompter.hpp"


#include "app/register_tools.hpp"

#include <unistd.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>


enum class Mode { Tui, Cli, Telegram };

// Mengisi `mode` dari argumen. false = argumen tidak dikenal (pesan sudah dicetak).
bool parse_mode(int argc, char** argv, Mode& mode) {
    if (argc > 2) {
        if (std::strcmp(argv[2], "--telegram") == 0) {
            mode = Mode::Telegram;
        } else if (std::strcmp(argv[2], "--cli") == 0) {
            mode = Mode::Cli;
        } else {
            std::cerr << "Argumen tidak dikenal: " << argv[2]
                      << "\nPakai: luna [config.json] [--cli | --telegram]\n";
            return false;
        }
    }
    return true;
}


// Urutan deklarasi penting: yang dirujuk ChatSession harus
// dideklarasikan lebih dulu (dihancurkan belakangan).
// App juga ModelSwitcher: /model memanggil switch_to() lewat antarmuka itu.
struct App : ModelSwitcher {
    Config cfg;
    std::unique_ptr<LLMBackend> backend;
    std::unique_ptr<Memory> memory;
    TelegramPrompter telegram_prompter;
    TerminalPrompter terminal_prompter;
    TuiPrompter tui_prompter;

    TerminalFileSink terminal_sink;
    TelegramFileSink telegram_sink;

    std::unique_ptr<SearchProvider> search_provider;

    std::unique_ptr<PathGuard> guard;
    std::unique_ptr<PermissionGate> gate;
    std::unique_ptr<ToolRegistry> registry;
    std::unique_ptr<ChatSession> session;   // paling akhir = dihancurkan paling dulu

    const std::vector<ModelChoice>& choices() const override { return cfg.models; }
    std::size_t active() const override { return cfg.active_index; }
    std::string label() const override { return cfg.active_provider + " · " + cfg.model; }
    std::string switch_to(std::size_t index) override;
};

// Ganti model saat berjalan. Gagal = cfg dan backend lama tidak berubah.
std::string App::switch_to(std::size_t index) {
    try {
        Config next = cfg;
        apply_model(next, index);
        std::unique_ptr<LLMBackend> fresh = create_backend(next);

        cfg = next;                      // ChatSession merujuk cfg ini (max_context, history_limit)
        session->set_backend(*fresh);    // sesi pindah dulu...
        backend = std::move(fresh);      // ...baru backend lama dibuang

        logger::info("app", "ganti model provider=" + cfg.active_provider +
                            " model=" + cfg.model);
    } catch (const std::exception& e) {
        logger::warn("app", std::string("ganti model gagal: ") + e.what());
        return e.what();
    }
    return "";
}

bool start_app(App& app, const std::string& config_path, Mode mode) {
    Prompter& prompter = (mode == Mode::Telegram) ? static_cast<Prompter&>(app.telegram_prompter)
                       : (mode == Mode::Cli)      ? static_cast<Prompter&>(app.terminal_prompter)
                                                  : static_cast<Prompter&>(app.tui_prompter);

    FileSink& file_sink = mode == Mode::Telegram ? static_cast<FileSink&>(app.telegram_sink)
                                                 : static_cast<FileSink&>(app.terminal_sink);
    
    try {
        app.cfg = load_config(config_path);
        logger::init(app.cfg.log_file, parse_log_level(app.cfg.log_level),
             static_cast<std::size_t>(app.cfg.log_max_kb) * 1024);
        
        
        app.backend = create_backend(app.cfg);
        app.memory = std::make_unique<Memory>(app.cfg.memory_db);
        app.search_provider = make_search_provider(app.cfg);

        app.gate = std::make_unique<PermissionGate>(make_policy(app.cfg.permissions), prompter);
        app.registry = std::make_unique<ToolRegistry>(*app.gate);

        register_tools(*app.registry, app.cfg, app.guard, file_sink, app.search_provider.get(), mode == Mode::Telegram);

        app.registry->set_profiles(app.cfg.profiles);
        if (!app.cfg.active_profile.empty()) {
            std::vector<std::string> unknown;
            app.registry->use_profile(app.cfg.active_profile, unknown);
            for (const auto& n : unknown) {
                logger::warn("app", "profil: tool tidak terdaftar: " + n);
            }
        }

        app.session = std::make_unique<ChatSession>(app.cfg, *app.backend, *app.memory, *app.registry);
        app.session->load();

        std::string tool_names;
        for (const auto& t : app.registry->schema()) {
            if (!tool_names.empty()) tool_names += ",";
            tool_names += t.value("name", "?");
        }
        const char* mode_name = (mode == Mode::Telegram) ? "telegram" :
                                (mode == Mode::Cli ? "cli" : "tui");
        logger::info("app", std::string("start mode=") + mode_name +
                            " backend=" + app.backend->name() + " model=" + app.cfg.model +
                            " history=" + std::to_string(app.session->history_size()));
        logger::info("app", "tools=" + tool_names);

        std::cerr << "allowed_dirs=" << app.cfg.allowed_dirs.size()
          << " tools=" << app.registry->schema().size() << '\n';
    } catch (const std::exception& e) {
        // std::cerr << "Gagal start: " << e.what() << '\n';
        logger::error("app", std::string("Gagal start: ") + e.what());
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    const std::string config_path = (argc > 1) ? argv[1] : "config.json";

    Mode mode = Mode::Tui;
    if(!parse_mode(argc, argv, mode)) { // jika false berarti 0, negasi menjadi true, jadi akan selesai dengan return 1
        return 1;
    }

    if (mode == Mode::Tui && (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))) {
        std::cerr << "TUI butuh terminal interaktif. Pakai --cli untuk input dari pipe.\n";
        return 1;
    }

    App app;
    if(!start_app(app, config_path, mode)) { // jika false berarti 0, negsi jadi true, jadi akan selesai program dengan return 1
        return 1;
    }


    if (mode == Mode::Telegram) {
        try {
            run_telegram(app.cfg, *app.session, *app.memory, app.telegram_prompter, app.telegram_sink);
        } catch (const std::exception& e) {
            // std::cerr << "Gagal: " << e.what() << '\n';
            logger::error("app", std::string("Gagal: ") + e.what());
            return 1;
        }
        return 0;
    }

    if (mode == Mode::Tui) {
        const std::string status = app.backend->name() + " · " + app.cfg.model;
        const int rc = run_tui(*app.session, *app.memory, app.tui_prompter, status,
                               *app.registry, app.cfg.input_history_max, &app);
        logger::info("app", "berhenti");
        return rc;
    }

    return run_cli(*app.backend, app.cfg, *app.session, *app.memory, *app.registry, app);
}