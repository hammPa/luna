#pragma once

#include "tools/path_guard.hpp"  // arg_string
#include "tools/tool.hpp"
#include "utils/process_utils.hpp"
#include "utils/string_utils.hpp"

#include <stdexcept>
#include <string>

class FocusWindowTool : public Tool {
public:
    std::string name() const override { return "focus_window"; }

    std::string description() const override {
        return "Memindahkan fokus ke jendela lain: 'direction' (left, right, up, down) relatif dari "
               "jendela yang sedang fokus, ATAU 'address' dari list_windows. Isi salah satu.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"direction", {{"type", "string"}, {"enum", {"left", "right", "up", "down"}}}},
                  {"address", {{"type", "string"},
                               {"description", "Alamat jendela dari list_windows, mis. 0x55d0c8a1b2c0"}}}}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            const std::string d = arg_string(args, "direction", "");
            if (!d.empty()) return "Pindah fokus ke " + d;
            return "Fokus ke jendela " + arg_string(args, "address", "?");
        } catch (const std::exception&) {
            return "Pindah fokus (argumen tidak valid)";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        const std::string dir = arg_string(args, "direction", "");
        const std::string addr = arg_string(args, "address", "");
        if (dir.empty() == addr.empty()) {
            throw std::runtime_error("Isi tepat salah satu: 'direction' atau 'address'.");
        }

        if (!dir.empty()) {
            std::string code;
            if (dir == "left") code = "l";
            else if (dir == "right") code = "r";
            else if (dir == "up") code = "u";
            else if (dir == "down") code = "d";
            else throw std::runtime_error("direction harus left, right, up, atau down.");
            dispatch({"movefocus", code});
        } else {
            if (!valid_address(addr)) throw std::runtime_error("Format alamat tidak valid.");
            if (!window_exists(addr)) {
                throw std::runtime_error("Jendela tidak ditemukan. Panggil list_windows untuk alamat terbaru.");
            }
            dispatch({"focuswindow", "address:" + addr});
        }
        return "Fokus sekarang: " + active_label();
    }

private:
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

    static void dispatch(const std::vector<std::string>& args) {
        std::vector<std::string> cmd = {"hyprctl", "dispatch"};
        cmd.insert(cmd.end(), args.begin(), args.end());
        CaptureResult r = run_capture(cmd);
        const std::string out = trim(r.out);
        if (r.exit_code != 0 || out != "ok") {
            throw std::runtime_error("hyprctl gagal: " + (out.empty() ? std::string("tanpa pesan") : out));
        }
    }

    static bool window_exists(const std::string& addr) {
        const nlohmann::json clients = query({"clients", "-j"});
        if (!clients.is_array()) return false;
        for (const auto& w : clients) {
            if (w.is_object() && w.value("mapped", true) &&
                w.value("address", std::string()) == addr) {
                return true;
            }
        }
        return false;
    }

    static std::string active_label() {
        try {
            const nlohmann::json j = query({"activewindow", "-j"});
            if (j.is_object() && j.contains("address")) {
                std::string title = j.value("title", std::string());
                if (title.size() > 80) title = title.substr(0, 80) + "...";
                int ws = 0;
                if (j.contains("workspace") && j["workspace"].is_object()) {
                    ws = j["workspace"].value("id", 0);
                }
                return j.value("class", std::string("?")) + ": " + title + " (ws " + std::to_string(ws) + ")";
            }
        } catch (const std::exception&) {
        }
        return "(tidak ada jendela yang fokus)";
    }
};