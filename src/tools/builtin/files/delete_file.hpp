#pragma once

#include "tools/path_guard.hpp"
#include "tools/tool.hpp"

#include <ctime>
#include <filesystem>
#include <stdexcept>

class DeleteFileTool : public Tool {
public:
    explicit DeleteFileTool(const PathGuard& guard) : guard_(guard) {}

    std::string name() const override { return "delete_file"; }

    std::string description() const override {
        return "Menghapus satu file di workspace dengan memindahkannya ke folder .trash "
               "(bisa dipulihkan). Hanya file, bukan folder.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"path", {{"type", "string"}, {"description", "Path relatif file yang dihapus"}}}}},
                {"required", {"path"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Destructive; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            namespace fs = std::filesystem;
            fs::path file = guard_.resolve(arg_string(args, "path", ""));
            std::error_code ec;
            std::string size = fs::is_regular_file(file, ec)
                                   ? std::to_string(fs::file_size(file, ec)) + " byte"
                                   : "tidak ada atau bukan file";
            return "MENGHAPUS " + file.string() + " (" + size + "), dipindah ke .trash";
        } catch (const std::exception& e) {
            return std::string("Menghapus file (tidak valid: ") + e.what() + ")";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        namespace fs = std::filesystem;
        const fs::path file = guard_.resolve(arg_string(args, "path", ""));
        const fs::path trash = guard_.workspace() / ".trash";

        if (file == guard_.workspace() || file == trash) {
            throw std::runtime_error("Folder ini tidak boleh dihapus.");
        }
        // Isi .trash tidak boleh dihapus lewat Luna (itu penghapusan permanen).
        const fs::path rel = file.lexically_relative(trash);
        if (!rel.empty() && *rel.begin() != "..") {
            throw std::runtime_error("File ini sudah ada di .trash.");
        }

        std::error_code ec;
        if (!fs::exists(file, ec)) {
            throw std::runtime_error("File tidak ada. Gunakan list_dir untuk melihat file yang tersedia.");
        }
        if (!fs::is_regular_file(file, ec)) {
            throw std::runtime_error("Hanya file biasa yang bisa dihapus (folder belum didukung).");
        }

        fs::create_directories(trash, ec);
        if (ec) throw std::runtime_error("Gagal membuat .trash: " + ec.message());

        fs::path dest = trash / (timestamp() + "_" + file.filename().string());
        for (int i = 1; fs::exists(dest, ec) && i < 100; ++i) {  // hindari bentrok nama
            dest = trash / (timestamp() + "_" + std::to_string(i) + "_" + file.filename().string());
        }

        fs::rename(file, dest, ec);
        if (ec) {
            throw std::runtime_error("Gagal memindahkan ke .trash: " + ec.message());
        }
        return "File dipindah ke " + dest.string() + " (bisa dipulihkan dengan memindahkannya kembali).";
    }

private:
    static std::string timestamp() {
        std::time_t now = std::time(nullptr);
        std::tm tm_buf{};
        localtime_r(&now, &tm_buf);
        char buf[32];
        if (std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm_buf) == 0) return "waktu";
        return buf;
    }

    const PathGuard& guard_;
};