#include "llm/backend_factory.hpp"
#include "utils/config.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

void trim_history(std::vector<Message>& history, std::size_t limit) {
    while (history.size() - 1 > limit) {
        history.erase(history.begin() + 1);
    }
    while (history.size() > 1 && history[1].role != "user") {
        history.erase(history.begin() + 1);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string config_path = (argc > 1) ? argv[1] : "config.json";

    Config cfg;
    std::unique_ptr<LLMBackend> backend;
    try {
        cfg = load_config(config_path);
        backend = create_backend(cfg);
    } catch (const std::exception& e) {
        std::cerr << "Gagal start: " << e.what() << '\n';
        return 1;
    }

    std::vector<Message> history;
    history.push_back({"system", cfg.system_prompt, ""});

    std::cout << "Backend: " << backend->name() << " | Model: " << cfg.model
              << "\nKetik 'exit' atau 'quit' untuk keluar.\n";

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        history.push_back({"user", line, ""});
        trim_history(history, cfg.history_limit);

        try {
            LLMResponse r = backend->chat(history, nlohmann::json::array());
            std::cout << r.text << "\n\n";
            history.push_back({"assistant", r.text, ""});
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            history.pop_back();
        }
    }

    std::cout << "Sampai jumpa.\n";
    return 0;
}