#pragma once

#include "tools/path_guard.hpp"
#include "tools/tool.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

class ReadFileTool : public Tool {
public:
    explicit ReadFileTool(const PathGuard& guard) : guard_(guard) {}

    std::string name() const override { return "read_file"; }

    std::string description() const override {
        return "Membaca isi file teks di workspace. File besar dipotong di 64 KB.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"path", {{"type", "string"},
                            {"description", "Path relatif file, mis. 'catatan.txt'"}}}}},
                {"required", {"path"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            return "Membaca file " + arg_string(args, "path", "?");
        } catch (const std::exception&) {
            return "Membaca file (argumen tidak valid)";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        namespace fs = std::filesystem;
        const std::string user_path = arg_string(args, "path", "");
        const fs::path file = guard_.resolve(user_path);  // "" ditolak di sini

        std::error_code ec;
        if (!fs::exists(file, ec)) {
            throw std::runtime_error("File tidak ada. Gunakan list_dir untuk melihat file yang tersedia.");
        }
        if (!fs::is_regular_file(file, ec)) {
            throw std::runtime_error("Bukan file biasa. Jika itu folder, gunakan list_dir.");
        }

        const std::uintmax_t size = fs::file_size(file, ec);
        if (ec) throw std::runtime_error("Gagal membaca ukuran file: " + ec.message());

        std::ifstream in(file, std::ios::binary);
        if (!in) throw std::runtime_error("Gagal membuka file.");

        const std::size_t want = static_cast<std::size_t>(std::min<std::uintmax_t>(size, kMaxBytes));
        std::string data(want, '\0');
        in.read(data.data(), static_cast<std::streamsize>(want));
        data.resize(static_cast<std::size_t>(in.gcount()));

        if (data.find('\0') != std::string::npos) {
            throw std::runtime_error("File ini biner, bukan teks, jadi tidak dibaca.");
        }
        if (data.empty()) return "(file kosong)";

        if (size > kMaxBytes) {
            data += "\n[dipotong: ukuran asli " + std::to_string(size) + " byte, dibaca " +
                    std::to_string(kMaxBytes) + " byte pertama]";
        }
        return data;
    }

private:
    static constexpr std::size_t kMaxBytes = 64 * 1024;
    const PathGuard& guard_;
};