#pragma once

#include "tools/path_guard.hpp"  // arg_string
#include "tools/tool.hpp"
#include "utils/process_utils.hpp"
#include "utils/string_utils.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

class CloseWindowTool : public Tool {
public:
    // allow_active=false: 'address' wajib diisi (dipakai di mode Telegram, karena
    // "jendela yang fokus" ada di layar yang tidak sedang kamu lihat).
    explicit CloseWindowTool(bool allow_active) : allow_active_(allow_active) {}

    std::string name() const override { return "close_window"; }

    std::string description() const override {
        std::string d = "Menutup SATU jendela (bukan seluruh aplikasi) dengan 'address' dari list_windows. ";
        d += allow_active_ ? "Tanpa 'address', yang ditutup adalah jendela yang sedang fokus. "
                           : "'address' wajib diisi. ";
        d += "Pekerjaan yang belum disimpan di jendela itu bisa hilang. "
             "Untuk menutup semua jendela satu aplikasi gunakan close_app.";
        return d;
    }

    nlohmann::json parameters() const override {
        nlohmann::json p = {{"type", "object"},
                            {"properties",
                             {{"address", {{"type", "string"},
                                           {"description", "Alamat jendela dari list_windows"}}}}}};
        if (!allow_active_) p["required"] = {"address"};
        return p;
    }

    // Bisa menghilangkan data yang belum disimpan = Destructive (tanpa "always").
    Risk risk(const nlohmann::json&) const override { return Risk::Destructive; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            const Win w = resolve(args);
            return "MENUTUP jendela " + w.label + " (ws " + std::to_string(w.workspace) +
                   "). Pekerjaan yang belum disimpan bisa hilang.";
        } catch (const std::exception& e) {
            return std::string("Menutup jendela (tidak valid: ") + e.what() + ")";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        const Win w = resolve(args);  // diselesaikan ulang saat eksekusi
        dispatch({"closewindow", "address:" + w.address});

        // Verifikasi: aplikasi bisa menunda (dialog "simpan perubahan?").
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            if (!find(w.address)) return "Jendela '" + w.label + "' sudah ditutup.";
        }
        return "Permintaan tutup dikirim ke '" + w.label +
               "', tapi jendela masih terbuka (mungkin menunggu konfirmasi aplikasi). Cek layar.";
    }

private:
    struct Win {
        std::string address;
        std::string label;
        int workspace = 0;
    };

    bool allow_active_;

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

    static Win to_win(const nlohmann::json& w) {
        Win x;
        x.address = w.value("address", std::string());
        std::string title = w.value("title", std::string());
        if (title.size() > 80) title = title.substr(0, 80) + "...";
        x.label = w.value("class", std::string("?")) + ": " + title;
        if (w.contains("workspace") && w["workspace"].is_object()) {
            x.workspace = w["workspace"].value("id", 0);
        }
        return x;
    }

    // Jendela dengan alamat itu, hanya jika benar-benar ada sekarang.
    static bool find(const std::string& addr, Win* out = nullptr) {
        const nlohmann::json clients = query({"clients", "-j"});
        if (!clients.is_array()) return false;
        for (const auto& w : clients) {
            if (w.is_object() && w.value("mapped", true) &&
                w.value("address", std::string()) == addr) {
                if (out) *out = to_win(w);
                return true;
            }
        }
        return false;
    }

    Win resolve(const nlohmann::json& args) const {
        const std::string addr = arg_string(args, "address", "");
        Win w;
        if (!addr.empty()) {
            if (!valid_address(addr)) throw std::runtime_error("Format alamat tidak valid.");
            if (!find(addr, &w)) {
                throw std::runtime_error("Jendela tidak ditemukan. Panggil list_windows untuk alamat terbaru.");
            }
            return w;
        }
        if (!allow_active_) {
            throw std::runtime_error("Dari kanal ini 'address' wajib diisi. Panggil list_windows dulu.");
        }
        const nlohmann::json j = query({"activewindow", "-j"});
        if (!j.is_object() || !j.contains("address")) {
            throw std::runtime_error("Tidak ada jendela yang sedang fokus.");
        }
        w = to_win(j);
        if (!valid_address(w.address)) throw std::runtime_error("Tidak ada jendela yang sedang fokus.");
        return w;
    }
};