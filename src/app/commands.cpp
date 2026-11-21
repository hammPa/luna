#include "app/commands.hpp"

#include "utils/string_utils.hpp"
#include "utils/ctx_format.hpp"
#include "tools/tool_registry.hpp"

#include <iostream>
#include <sstream>

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

void cmd_clear(ChatSession& session, std::ostream& out) {
    try {
        session.clear();
        out << "Riwayat dihapus. Fakta tetap tersimpan.\n\n";
    } catch (const std::exception& e) {
        std::cerr << "Gagal menghapus riwayat: " << e.what() << '\n';
    }
}

void cmd_ctx(const ChatSession& session, std::ostream& out) {
    const int used = session.ctx_used();
    if (used < 0) {
        out << "Konteks: belum ada (kirim satu pesan dulu).\n\n";
        return;
    }
    out << ctx_fmt::summary(used, session.ctx_max(), session.ctx_estimated()) << ' '
        << ctx_fmt::bar(ctx_fmt::percent(used, session.ctx_max())) << '\n';
    if (session.ctx_estimated()) {
        out << "(~ = perkiraan, provider tidak mengirim usage)\n";
    }
    out << '\n';
}

void cmd_tools(const ToolRegistry& tools, std::ostream& out) {
    const auto all = tools.list();
    std::size_t on = 0;
    for (const auto& t : all) {
        out << (t.enabled ? "[on ] " : "[off] ") << t.name << '\n';
        if (t.enabled) ++on;
    }
    out << on << " dari " << all.size() << " tool aktif\n\n";
}

void cmd_tool(const std::string& arg, ToolRegistry& tools, std::ostream& out) {
    std::istringstream ss(arg);
    std::string action, name, extra;
    ss >> action >> name;
    if ((action != "on" && action != "off") || name.empty() || (ss >> extra)) {
        out << "Format: /tool on|off nama\n\n";
        return;
    }
    if (!tools.set_enabled(name, action == "on")) {
        out << "Tool tidak ada: " << name << " (lihat /tools)\n\n";
        return;
    }
    out << name << (action == "on" ? " diaktifkan" : " dimatikan") << ".\n\n";
}

}  // namespace

bool handle_command(const std::string& line, Memory& memory, ChatSession& session,
                    std::ostream& out, ToolRegistry* tools){
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
            "Perintah: /remember kunci=nilai, /facts, /clear\n";
        return true;
    }
    if (line == "/clear") {
        cmd_clear(session, out);
        return true;
    }
    if (line == "/ctx") {
        cmd_ctx(session, out);
        return true;
    }
    if (tools && line == "/tools") {
        cmd_tools(*tools, out);
        return true;
    }
    if (tools && (line == "/tool" || line.rfind("/tool ", 0) == 0)) {
        cmd_tool(line.substr(5), *tools, out);
        return true;
    }

    return false;
}