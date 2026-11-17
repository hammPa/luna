#pragma once

#include "utils/file_utils.hpp"
#include "tools/path_guard.hpp"
#include "tools/tool.hpp"

class CreateFileTool : public Tool {
public:
    explicit CreateFileTool(const PathGuard& guard) : guard_(guard) {}

    std::string name() const override { return "create_file"; }

    std::string description() const override {
        return "Membuat file baru di workspace dengan isi tertentu. Gagal jika file sudah ada "
               "(gunakan write_file untuk menimpa atau patch_file untuk mengubah sebagian).";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"path", {{"type", "string"}, {"description", "Path relatif file baru"}}},
                  {"content", {{"type", "string"}, {"description", "Isi file"}}}}},
                {"required", {"path", "content"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Write; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            std::string content = arg_string(args, "content", "");
            return "Membuat file " + guard_.resolve(arg_string(args, "path", "")).string() +
                   " (" + std::to_string(content.size()) + " byte): \"" + preview(content) + "\"";
        } catch (const std::exception& e) {
            return std::string("Membuat file (tidak valid: ") + e.what() + ")";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        namespace fs = std::filesystem;
        const fs::path file = guard_.resolve(arg_string(args, "path", ""));
        const std::string content = arg_string(args, "content", "");

        std::error_code ec;
        if (fs::exists(file, ec)) {
            throw std::runtime_error("File sudah ada. Gunakan write_file untuk menimpa "
                                     "atau patch_file untuk mengubah sebagian.");
        }
        write_file_atomic(file, content);
        return "Berhasil membuat " + file.string() + " (" + std::to_string(content.size()) + " byte).";
    }

private:
    const PathGuard& guard_;
};