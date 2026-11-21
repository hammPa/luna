#pragma once

#include "utils/file_utils.hpp"
#include "tools/path_guard.hpp"
#include "tools/tool.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

class AppendFileTool : public Tool {
public:
    explicit AppendFileTool(const PathGuard& guard) : guard_(guard) {}

    std::string name() const override { return "append_file"; }

    std::string description() const override {
        return "Menambahkan teks di akhir file yang SUDAH ADA di workspace, tanpa menimpa isinya. "
               "Untuk file baru pakai create_file.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"path", {{"type", "string"}, {"description", "Path relatif file"}}},
                  {"content", {{"type", "string"}, {"description", "Teks yang ditambahkan"}}}}},
                {"required", {"path", "content"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Write; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            const std::filesystem::path file = guard_.resolve(arg_string(args, "path", ""));
            const std::string content = arg_string(args, "content", "");
            return "Menambahkan ke " + file.string() + " (" + std::to_string(content.size()) +
                   " byte): \"" + preview(content) + "\"";
        } catch (const std::exception& e) {
            return std::string("Menambahkan ke file (tidak valid: ") + e.what() + ")";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        namespace fs = std::filesystem;
        const fs::path file = guard_.resolve(arg_string(args, "path", ""));
        const std::string content = arg_string(args, "content", "");

        std::error_code ec;
        if (!fs::exists(file, ec)) return "Error: file belum ada (pakai create_file): " + file.string();
        if (!fs::is_regular_file(file, ec)) return "Error: bukan file biasa: " + file.string();

        const std::uintmax_t size = fs::file_size(file, ec);
        if (ec) return "Error: gagal membaca ukuran file: " + ec.message();
        if (size + content.size() > kMaxWriteBytes) {
            return "Error: ukuran file akan melebihi batas (maks " +
                   std::to_string(kMaxWriteBytes) + " byte).";
        }

        std::ofstream out(file, std::ios::binary | std::ios::app);
        if (!out) return "Error: gagal membuka file untuk ditambah.";
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out) return "Error: gagal menulis ke file.";
        return "Berhasil menambahkan " + std::to_string(content.size()) + " byte ke " +
               file.string() + " (total " + std::to_string(size + content.size()) + " byte).";
    }

private:
    const PathGuard& guard_;
};