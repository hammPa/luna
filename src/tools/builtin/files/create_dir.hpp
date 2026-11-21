#pragma once

#include "tools/path_guard.hpp"
#include "tools/tool.hpp"

#include <filesystem>
#include <string>
#include <system_error>

class CreateDirTool : public Tool {
public:
    explicit CreateDirTool(const PathGuard& guard) : guard_(guard) {}

    std::string name() const override { return "create_dir"; }

    std::string description() const override {
        return "Membuat folder di workspace (termasuk folder induk yang belum ada). "
               "Tidak error jika folder sudah ada.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"path", {{"type", "string"}, {"description", "Path relatif folder"}}}}},
                {"required", {"path"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Write; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            return "Membuat folder " + guard_.resolve(arg_string(args, "path", "")).string();
        } catch (const std::exception& e) {
            return std::string("Membuat folder (tidak valid: ") + e.what() + ")";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        namespace fs = std::filesystem;
        const fs::path dir = guard_.resolve(arg_string(args, "path", ""));
        std::error_code ec;
        if (fs::is_directory(dir, ec)) return "Folder sudah ada: " + dir.string();
        if (fs::exists(dir, ec)) return "Error: sudah ada file dengan nama itu: " + dir.string();
        fs::create_directories(dir, ec);
        if (ec) return "Error: gagal membuat folder: " + ec.message();
        return "Berhasil membuat folder " + dir.string();
    }

private:
    const PathGuard& guard_;
};