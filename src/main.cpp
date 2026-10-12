#include "llm/backend_factory.hpp"
#include "utils/config.hpp"
#include "memory/memory.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

// Potong pesan lama (indeks 1 dst), system prompt di indeks 0 dipertahankan.
void trim_history(std::vector<Message>& history, std::size_t limit) {
    while (history.size() - 1 > limit) {
        history.erase(history.begin() + 1);
    }
    while (history.size() > 1 && history[1].role != "user") {
        history.erase(history.begin() + 1);
    }
}

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    std::size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    std::size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string build_system_prompt(
    const Config& cfg,
    const std::vector<std::pair<std::string, std::string>>& facts) {
    if (facts.empty()) return cfg.system_prompt;

    std::string out = cfg.system_prompt + "\n\nFakta tentang pengguna:\n";
    for (const auto& [k, v] : facts) {
        out += "- " + k + ": " + v + "\n";
    }
    return out;
}

// Mengembalikan true jika baris adalah perintah slash (sudah ditangani).
bool handle_command(const std::string& line, Memory& mem) {
    if (line.rfind("/remember ", 0) == 0) {
        std::string arg = line.substr(10);
        std::size_t eq = arg.find('=');
        if (eq == std::string::npos) {
            std::cout << "Format: /remember kunci=nilai\n\n";
            return true;
        }
        std::string key = trim(arg.substr(0, eq));
        std::string value = trim(arg.substr(eq + 1));
        if (key.empty() || value.empty()) {
            std::cout << "Kunci dan nilai tidak boleh kosong.\n\n";
            return true;
        }
        try {
            mem.set_fact(key, value);
            std::cout << "Tersimpan: " << key << " = " << value << "\n\n";
        } catch (const std::exception& e) {
            std::cerr << "Gagal menyimpan fakta: " << e.what() << '\n';
        }
        return true;
    }

    if (line == "/facts") {
        try {
            auto facts = mem.all_facts();
            if (facts.empty()) {
                std::cout << "(belum ada fakta)\n\n";
            }
            for (const auto& [k, v] : facts) {
                std::cout << "- " << k << ": " << v << '\n';
            }
            if (!facts.empty()) std::cout << '\n';
        } catch (const std::exception& e) {
            std::cerr << "Gagal membaca fakta: " << e.what() << '\n';
        }
        return true;
    }

    return false;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string config_path = (argc > 1) ? argv[1] : "config.json";

    Config cfg;
    std::unique_ptr<LLMBackend> backend;
    std::unique_ptr<Memory> mem;
    std::vector<Message> history;

    try {
        cfg = load_config(config_path);
        backend = create_backend(cfg);
        mem = std::make_unique<Memory>(cfg.memory_db);
        
        history.push_back({"system", cfg.system_prompt, ""});
        for (auto& m : mem->load_recent(cfg.history_limit)) {
            history.push_back(std::move(m));
        }
        trim_history(history, cfg.history_limit);

    } catch (const std::exception& e) {
        std::cerr << "Gagal start: " << e.what() << '\n';
        return 1;
    }

    std::cout << "Backend: " << backend->name() << " | Model: " << cfg.model
              << " | Riwayat dimuat: " << history.size() - 1 << " pesan"
              << "\nPerintah: /remember kunci=nilai, /facts, exit\n";

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        if (!line.empty() && line[0] == '/' && handle_command(line, *mem)) {
            continue;
        }

        // Fakta bisa berubah di tengah sesi, jadi system prompt dibangun ulang.
        try {
            history[0].content = build_system_prompt(cfg, mem->all_facts());
        } catch (const std::exception& e) {
            std::cerr << "Peringatan: gagal membaca fakta: " << e.what() << '\n';
            history[0].content = cfg.system_prompt;
        }

        history.push_back({"user", line, ""});
        trim_history(history, cfg.history_limit);

        LLMResponse r;
        try {
            r = backend->chat(history, nlohmann::json::array());
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            history.pop_back();  // buang pesan user yang gagal
            continue;
        }

        std::cout << r.text << "\n\n";
        history.push_back({"assistant", r.text, ""});

        // Simpan hanya setelah chat sukses. Gagal simpan tidak mematikan chat.
        try {
            mem->add_message({"user", line, ""});
            mem->add_message({"assistant", r.text, ""});
        } catch (const std::exception& e) {
            std::cerr << "Peringatan: gagal menyimpan ke database: " << e.what() << '\n';
        }
    }

    std::cout << "Sampai jumpa.\n";
    return 0;
}