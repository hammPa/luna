#pragma once

#include "tools/path_guard.hpp"  // arg_string
#include "tools/tool.hpp"
#include "utils/process_utils.hpp"

#include <stdexcept>
#include <string>

class ListWindowsTool : public Tool {
public:
    std::string name() const override { return "list_windows"; }

    std::string description() const override {
        return "Daftar jendela aplikasi yang terbuka di Hyprland. scope: 'all' (semua workspace), "
               "'workspace' (hanya workspace yang sedang fokus), 'active' (hanya jendela yang fokus). "
               "Setiap baris berisi alamat jendela, yang dipakai focus_window dan close_window.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"scope", {{"type", "string"},
                             {"enum", {"all", "workspace", "active"}},
                             {"description", "Default 'all'"}}}}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            return "Melihat jendela (" + arg_string(args, "scope", "all") + ")";
        } catch (const std::exception&) {
            return "Melihat jendela (argumen tidak valid)";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        const std::string scope = arg_string(args, "scope", "all");
        if (scope != "all" && scope != "workspace" && scope != "active") {
            throw std::runtime_error("scope harus 'all', 'workspace', atau 'active'.");
        }

        const nlohmann::json clients = query({"clients", "-j"});
        if (!clients.is_array() || clients.empty()) return "Tidak ada jendela.";

        const std::string active = active_address();
        if (scope == "active" && active.empty()) return "Tidak ada jendela yang sedang fokus.";

        int ws_filter = -1;
        if (scope == "workspace") {
            const nlohmann::json w = query({"activeworkspace", "-j"});
            ws_filter = w.is_object() ? w.value("id", -1) : -1;
        }

        std::string out;
        std::size_t shown = 0;
        for (const auto& w : clients) {
            if (!w.is_object() || !w.value("mapped", true)) continue;

            const std::string addr = w.value("address", std::string());
            if (!valid_address(addr)) continue;

            int ws = 0;
            if (w.contains("workspace") && w["workspace"].is_object()) {
                ws = w["workspace"].value("id", 0);
            }
            if (scope == "workspace" && ws != ws_filter) continue;
            if (scope == "active" && addr != active) continue;

            if (++shown > kMax) {
                out += "[dipotong, hanya " + std::to_string(kMax) + " pertama]\n";
                break;
            }
            std::string title = w.value("title", std::string());
            if (title.size() > 80) title = title.substr(0, 80) + "...";

            out += addr + " [ws " + std::to_string(ws) + "] " +
                   w.value("class", std::string("?")) + ": " + title;
            if (addr == active) out += "  <- FOKUS";
            out += "\n";
        }
        return out.empty() ? "Tidak ada jendela." : out;
    }

private:
    static constexpr std::size_t kMax = 40;

    // Alamat jendela: "0x" diikuti 1-16 digit heksadesimal.
    static bool valid_address(const std::string& a) {
        if (a.size() < 3 || a.size() > 18 || a[0] != '0' || a[1] != 'x') return false;
        for (std::size_t i = 2; i < a.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(a[i]);
            const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            if (!hex) return false;
        }
        return true;
    }

    static nlohmann::json query(const std::vector<std::string>& args) {
        std::vector<std::string> cmd = {"hyprctl"};
        cmd.insert(cmd.end(), args.begin(), args.end());
        CaptureResult r = run_capture(cmd);
        if (r.exit_code != 0) throw std::runtime_error("hyprctl gagal (kode " + std::to_string(r.exit_code) + ").");
        try {
            return nlohmann::json::parse(r.out);
        } catch (const nlohmann::json::exception&) {
            throw std::runtime_error("Balasan hyprctl tidak valid.");
        }
    }

    // Alamat jendela yang fokus, atau string kosong jika tidak ada.
    static std::string active_address() {
        try {
            const nlohmann::json j = query({"activewindow", "-j"});
            if (j.is_object() && j.contains("address") && j["address"].is_string()) {
                const std::string a = j["address"].get<std::string>();
                if (valid_address(a)) return a;
            }
        } catch (const std::exception&) {
        }
        return "";
    }
};