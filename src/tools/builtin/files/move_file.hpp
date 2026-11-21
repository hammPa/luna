#pragma once

#include "tools/path_guard.hpp"
#include "tools/tool.hpp"

#include <filesystem>
#include <string>
#include <system_error>

class MoveFileTool : public Tool {
public:
    explicit MoveFileTool(const PathGuard& guard) : guard_(guard) {}

    std::string name() const override { return "move_file"; }

    std::string description() const override {
        return "Memindahkan atau mengganti nama SATU file di workspace. Tidak menimpa: "
               "gagal jika tujuan sudah ada. Folder tujuan dibuat otomatis.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"from", {{"type", "string"}, {"description", "Path relatif file asal"}}},
                  {"to", {{"type", "string"}, {"description", "Path relatif tujuan (nama file baru)"}}}}},
                {"required", {"from", "to"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Write; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            return "Memindahkan " + guard_.resolve(arg_string(args, "from", "")).string() +
                   " -> " + guard_.resolve(arg_string(args, "to", "")).string();
        } catch (const std::exception& e) {
            return std::string("Memindahkan file (tidak valid: ") + e.what() + ")";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        namespace fs = std::filesystem;
        const fs::path from = guard_.resolve(arg_string(args, "from", ""));
        const fs::path to = guard_.resolve(arg_string(args, "to", ""));

        std::error_code ec;
        if (!fs::exists(from, ec)) return "Error: file asal tidak ada: " + from.string();
        if (!fs::is_regular_file(from, ec)) return "Error: hanya file biasa yang bisa dipindah: " + from.string();
        if (from == to) return "Error: asal dan tujuan sama.";
        if (fs::exists(to, ec)) return "Error: tujuan sudah ada, tidak menimpa: " + to.string();

        fs::create_directories(to.parent_path(), ec);
        if (ec) return "Error: gagal membuat folder tujuan: " + ec.message();

        fs::rename(from, to, ec);
        if (ec) return "Error: gagal memindahkan: " + ec.message();
        return "Berhasil memindahkan " + from.string() + " -> " + to.string();
    }

private:
    const PathGuard& guard_;
};