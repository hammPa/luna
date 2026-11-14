#pragma once

#include "tools/path_guard.hpp"  // arg_string
#include "tools/tool.hpp"
#include "utils/config.hpp"      // AppEntry
#include "utils/process_utils.hpp"

#include <chrono>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>

class CloseAppTool : public Tool {
public:
    explicit CloseAppTool(const std::map<std::string, AppEntry>& apps) {
        for (const auto& [k, e] : apps) {
            if (e.process.empty()) continue;  // tidak closable
            if (e.process.find(' ') != std::string::npos || e.process.front() == '-') {
                throw std::runtime_error("apps: 'process' untuk '" + k + "' tidak valid.");
            }
            closable_[k] = e.process;
        }
    }

    bool empty() const { return closable_.empty(); }

    std::string name() const override { return "close_app"; }

    std::string description() const override {
        std::string d = "Menutup SEMUA jendela satu aplikasi yang sedang berjalan dengan sopan (SIGTERM). "
                        "Pekerjaan yang belum disimpan bisa hilang. Untuk menutup satu jendela saja "
                        "gunakan close_window. Yang bisa ditutup: ";
        bool first = true;
        for (const auto& [k, p] : closable_) {
            d += (first ? "" : ", ") + k;
            first = false;
        }
        return d + ".";
    }

    nlohmann::json parameters() const override {
        nlohmann::json names = nlohmann::json::array();
        for (const auto& [k, p] : closable_) names.push_back(k);
        return {{"type", "object"},
                {"properties", {{"name", {{"type", "string"}, {"enum", names},
                                          {"description", "Nama aplikasi dari daftar"}}}}},
                {"required", {"name"}}};
    }

    // Bisa kehilangan data yang belum disimpan = Destructive (tanpa "always").
    Risk risk(const nlohmann::json&) const override { return Risk::Destructive; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            std::string n = arg_string(args, "name", "?");
            auto it = closable_.find(n);
            return "MENUTUP semua jendela aplikasi '" + n + "'" +
                   (it != closable_.end() ? " (proses: " + it->second + ")" : " (tidak ada di daftar)") +
                   ". Pekerjaan yang belum disimpan bisa hilang.";
        } catch (const std::exception&) {
            return "Menutup aplikasi (argumen tidak valid)";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        const std::string n = arg_string(args, "name", "");
        auto it = closable_.find(n);
        if (it == closable_.end()) {
            throw std::runtime_error("Aplikasi '" + n + "' tidak ada di daftar yang boleh ditutup.");
        }
        if (!program_exists("pkill") || !program_exists("pgrep")) {
            throw std::runtime_error("pkill atau pgrep tidak ditemukan di sistem.");
        }

        const std::string uid = std::to_string(getuid());
        const std::string& proc = it->second;

        const int rc = run_and_wait({"pkill", "-TERM", "-x", "-U", uid, proc});
        if (rc == 1) return "Aplikasi '" + n + "' sedang tidak berjalan.";
        if (rc != 0) throw std::runtime_error("pkill gagal (kode " + std::to_string(rc) + ").");

        // Verifikasi: tunggu sampai prosesnya benar-benar hilang (maks ~1,5 detik).
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            if (run_and_wait({"pgrep", "-x", "-U", uid, proc}) == 1) {
                return "Aplikasi '" + n + "' sudah ditutup.";
            }
        }
        return "Permintaan tutup dikirim ke '" + n + "', tapi masih berjalan (mungkin menunggu "
               "konfirmasi seperti simpan dokumen). Cek layar, atau pakai list_windows dan "
               "close_window untuk menutup satu jendela.";
    }

private:
    std::map<std::string, std::string> closable_;  // nama -> proses
};