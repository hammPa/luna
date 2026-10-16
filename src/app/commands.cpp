#include "app/commands.hpp"

#include "utils/string_utils.hpp"

#include <iostream>

namespace {

void cmd_remember(const std::string& arg, Memory& memory, std::ostream& out) {
    std::size_t eq = arg.find('=');
    if (eq == std::string::npos) {
        out << "Format: /remember kunci=nilai\n\n";
        return;
    }
    std::string key = trim(arg.substr(0, eq));
    std::string value = trim(arg.substr(eq + 1));
    if (key.empty() || value.empty()) {
        out << "Kunci dan nilai tidak boleh kosong.\n\n";
        return;
    }
    try {
        memory.set_fact(key, value);
        out << "Tersimpan: " << key << " = " << value << "\n\n";
    } catch (const std::exception& e) {
        std::cerr << "Gagal menyimpan fakta: " << e.what() << '\n';
    }
}

void cmd_facts(Memory& memory, std::ostream& out) {
    try {
        auto facts = memory.all_facts();
        if (facts.empty()) {
            out << "(belum ada fakta)\n\n";
            return;
        }
        for (const auto& [k, v] : facts) {
            out << "- " << k << ": " << v << '\n';
        }
        out << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Gagal membaca fakta: " << e.what() << '\n';
    }
}

}  // namespace

bool handle_command(const std::string& line, Memory& memory, std::ostream& out) {
    if (line.rfind("/remember ", 0) == 0) {
        cmd_remember(line.substr(10), memory, out);
        return true;
    }
    if (line == "/facts") {
        cmd_facts(memory, out);
        return true;
    }
    if (line == "/start" || line == "/help") {
        out << "Halo, saya Luna. Kirim pesan biasa untuk ngobrol atau menjalankan tool.\n"
            "Perintah: /remember kunci=nilai, /facts\n";
        return true;
    }
    return false;
}