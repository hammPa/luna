#pragma once

#include "tools/file_utils.hpp"
#include "tools/path_guard.hpp"
#include "tools/tool.hpp"

class WriteFileTool : public Tool {
public:
    explicit WriteFileTool(const PathGuard& guard) : guard_(guard) {}

    std::string name() const override { return "write_file"; }

    std::string description() const override {
        return "Menulis isi file di workspace; MENIMPA jika file sudah ada. "
               "Untuk file baru lebih baik create_file, untuk perubahan kecil patch_file.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"path", {{"type", "string"}, {"description", "Path relatif file"}}},
                  {"content", {{"type", "string"}, {"description", "Isi lengkap file"}}}}},
                {"required", {"path", "content"}}};
    }

    // Menimpa file yang sudah ada = Destructive (tanpa opsi "always").
    Risk risk(const nlohmann::json& args) const override {
        try {
            std::error_code ec;
            if (std::filesystem::exists(guard_.resolve(arg_string(args, "path", "")), ec)) {
                return Risk::Destructive;
            }
        } catch (const std::exception&) {
        }
        return Risk::Write;
    }

    std::string describe(const nlohmann::json& args) const override {
        try {
            namespace fs = std::filesystem;
            fs::path file = guard_.resolve(arg_string(args, "path", ""));
            std::string content = arg_string(args, "content", "");
            std::error_code ec;
            std::string head;
            if (fs::exists(file, ec)) {
                head = "MENIMPA " + file.string() + " (" +
                       std::to_string(fs::file_size(file, ec)) + " byte -> ";
            } else {
                head = "Membuat " + file.string() + " (";
            }
            return head + std::to_string(content.size()) + " byte): \"" + preview(content) + "\"";
        } catch (const std::exception& e) {
            return std::string("Menulis file (tidak valid: ") + e.what() + ")";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        const std::filesystem::path file = guard_.resolve(arg_string(args, "path", ""));
        const std::string content = arg_string(args, "content", "");
        write_file_atomic(file, content);
        return "Berhasil menulis " + file.string() + " (" + std::to_string(content.size()) + " byte).";
    }

private:
    const PathGuard& guard_;
};