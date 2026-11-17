#pragma once

#include "utils/file_utils.hpp"
#include "tools/path_guard.hpp"
#include "tools/tool.hpp"

class PatchFileTool : public Tool {
public:
    explicit PatchFileTool(const PathGuard& guard) : guard_(guard) {}

    std::string name() const override { return "patch_file"; }

    std::string description() const override {
        return "Mengganti satu potongan teks di file yang sudah ada. 'old' harus muncul "
               "tepat satu kali di file; sertakan cukup konteks agar unik.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"path", {{"type", "string"}, {"description", "Path relatif file"}}},
                  {"old", {{"type", "string"}, {"description", "Teks lama (harus unik)"}}},
                  {"new", {{"type", "string"}, {"description", "Teks pengganti"}}}}},
                {"required", {"path", "old", "new"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Write; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            return "Mengubah " + guard_.resolve(arg_string(args, "path", "")).string() +
                   "\n    dari: \"" + preview(arg_string(args, "old", "")) + "\"" +
                   "\n    jadi: \"" + preview(arg_string(args, "new", "")) + "\"";
        } catch (const std::exception& e) {
            return std::string("Mengubah file (tidak valid: ") + e.what() + ")";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        namespace fs = std::filesystem;
        const fs::path file = guard_.resolve(arg_string(args, "path", ""));
        const std::string old_s = arg_string(args, "old", "");
        const std::string new_s = arg_string(args, "new", "");

        if (old_s.empty()) throw std::runtime_error("'old' tidak boleh kosong.");

        std::error_code ec;
        if (!fs::is_regular_file(file, ec)) {
            throw std::runtime_error("File tidak ada. Gunakan list_dir, atau create_file untuk file baru.");
        }
        if (fs::file_size(file, ec) > kMaxWriteBytes) {
            throw std::runtime_error("File terlalu besar untuk di-patch.");
        }

        std::ifstream in(file, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        std::size_t pos = data.find(old_s);
        if (pos == std::string::npos) {
            throw std::runtime_error("Teks 'old' tidak ditemukan. Baca file dulu dengan read_file "
                                     "dan salin teks persis (termasuk spasi).");
        }
        if (data.find(old_s, pos + old_s.size()) != std::string::npos) {
            throw std::runtime_error("Teks 'old' muncul lebih dari sekali (ambigu). "
                                     "Sertakan lebih banyak konteks di sekitarnya.");
        }

        data.replace(pos, old_s.size(), new_s);
        write_file_atomic(file, data);
        return "Berhasil mengubah " + file.string() + ".";
    }

private:
    const PathGuard& guard_;
};