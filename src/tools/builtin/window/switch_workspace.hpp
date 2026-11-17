#pragma once

#include "tools/tool.hpp"
#include "utils/process_utils.hpp"
#include "utils/string_utils.hpp"

#include <cstdlib>
#include <stdexcept>

class SwitchWorkspaceTool : public Tool {
public:
    // Tool hanya layak didaftarkan jika ini true.
    static bool available() {
        const char* sig = std::getenv("HYPRLAND_INSTANCE_SIGNATURE");
        return sig != nullptr && *sig != '\0' && program_exists("hyprctl");
    }

    std::string name() const override { return "switch_workspace"; }

    std::string description() const override {
        return "Pindah ke workspace desktop Hyprland nomor 1 sampai 10. "
               "Hanya memindahkan tampilan, tidak membuka atau menutup aplikasi.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"workspace", {{"type", "integer"},
                                 {"minimum", 1},
                                 {"maximum", 10},
                                 {"description", "Nomor workspace tujuan (1-10)"}}}}},
                {"required", {"workspace"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            return "Pindah ke workspace " + std::to_string(parse(args));
        } catch (const std::exception&) {
            return "Pindah workspace (argumen tidak valid)";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        const int n = parse(args);
        if (!available()) {
            throw std::runtime_error("Hyprland tidak terdeteksi (hyprctl atau "
                                     "HYPRLAND_INSTANCE_SIGNATURE tidak ada).");
        }
        CaptureResult r = run_capture({"hyprctl", "dispatch", "workspace", std::to_string(n)});
        const std::string out = trim(r.out);
        if (r.exit_code != 0 || out != "ok") {
            throw std::runtime_error("hyprctl gagal: " + (out.empty() ? "tanpa pesan" : out));
        }
        return "Berhasil pindah ke workspace " + std::to_string(n) + ".";
    }

private:
    // Model kadang mengirim angka sebagai string ("3"), jadi keduanya diterima.
    static int parse(const nlohmann::json& args) {
        if (!args.is_object() || !args.contains("workspace")) {
            throw std::runtime_error("Argumen 'workspace' wajib diisi (angka 1-10).");
        }
        const auto& w = args["workspace"];
        long long n = 0;
        if (w.is_number_integer()) {
            n = w.get<long long>();
        } else if (w.is_string()) {
            const std::string s = trim(w.get<std::string>());
            if (s.empty() || s.size() > 3) throw std::runtime_error("Nomor workspace tidak valid.");
            for (unsigned char c : s) {
                if (c < '0' || c > '9') throw std::runtime_error("Nomor workspace harus angka.");
            }
            n = std::stoll(s);
        } else {
            throw std::runtime_error("Nomor workspace harus berupa angka 1-10.");
        }
        if (n < 1 || n > 10) throw std::runtime_error("Nomor workspace harus 1 sampai 10.");
        return static_cast<int>(n);
    }
};