#pragma once

#include "tools/path_guard.hpp"
#include "tools/tool.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

class ListDirTool : public Tool {
public:
    // Menyimpan referensi: PathGuard harus hidup lebih lama dari tool ini.
    explicit ListDirTool(const PathGuard& guard) : guard_(guard) {}

    std::string name() const override { return "list_dir"; }

    std::string description() const override {
        return "Menampilkan isi sebuah folder di workspace (nama, tipe, ukuran). "
               "Gunakan path '.' untuk folder workspace utama.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"path", {{"type", "string"},
                            {"description", "Path relatif folder, default '.'"}}}}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            return "Melihat isi folder " + arg_string(args, "path", ".");
        } catch (const std::exception&) {
            return "Melihat isi folder (argumen tidak valid)";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        namespace fs = std::filesystem;
        const fs::path dir = guard_.resolve(arg_string(args, "path", "."));

        std::error_code ec;
        if (!fs::exists(dir, ec)) {
            throw std::runtime_error("Folder tidak ada. Gunakan list_dir dengan path '.' "
                                     "untuk melihat isi workspace.");
        }
        if (!fs::is_directory(dir, ec)) {
            throw std::runtime_error("Itu file, bukan folder. Gunakan read_file untuk membacanya.");
        }

        std::vector<std::string> lines;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            std::error_code e2;
            std::string name = entry.path().filename().string();
            if (entry.is_directory(e2)) {
                lines.push_back(name + "/  [dir]");
            } else {
                auto size = entry.file_size(e2);
                lines.push_back(name + "  [file, " +
                                (e2 ? std::string("?") : std::to_string(size)) + " byte]");
            }
        }
        if (ec) {
            throw std::runtime_error("Gagal membaca folder: " + ec.message());
        }

        std::sort(lines.begin(), lines.end());
        if (lines.empty()) return "(folder kosong)";

        std::string out;
        const std::size_t shown = std::min(lines.size(), kMaxEntries);
        for (std::size_t i = 0; i < shown; ++i) out += lines[i] + '\n';
        if (lines.size() > kMaxEntries) {
            out += "[dipotong: " + std::to_string(lines.size()) + " entri, hanya " +
                   std::to_string(kMaxEntries) + " ditampilkan]\n";
        }
        return out;
    }

private:
    static constexpr std::size_t kMaxEntries = 200;
    const PathGuard& guard_;
};