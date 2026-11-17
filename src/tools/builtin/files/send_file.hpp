#pragma once

#include "tools/file_blocklist.hpp"
#include "tools/file_sink.hpp"
#include "tools/path_guard.hpp"  // PathGuard, arg_string
#include "tools/tool.hpp"
#include "utils/format_utils.hpp"

#include <cstdint>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Mengirim file dari workspace ke pengguna.
//   Telegram : file DIUNGGAH ke chat (risk Exfiltrate: selalu ditanya, TOTP dari Telegram).
//   Terminal : hanya menunjukkan lokasi file (risk Safe, tidak ada data keluar).
// Isi file tidak pernah dibaca ke model.
class SendFileTool : public Tool {
public:
    SendFileTool(const PathGuard& guard, FileSink& sink, std::uintmax_t max_bytes)
        : guard_(guard), sink_(sink), max_bytes_(max_bytes) {}

    std::string name() const override { return "send_file"; }

    std::string description() const override {
        return "Mengirim 1 sampai 5 file dari workspace ke pengguna. Di Telegram file diunggah ke chat "
               "(pengguna harus menyetujui dengan kode authenticator); di terminal hanya menunjukkan "
               "lokasi file. Isi file tidak dibaca. Cari path yang benar dulu dengan search_files atau "
               "list_dir. File rahasia (kunci, .env, database) selalu ditolak.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"paths", {{"type", "array"},
                             {"items", {{"type", "string"}}},
                             {"minItems", 1},
                             {"maxItems", static_cast<int>(kMaxFiles)},
                             {"description", "Path relatif file yang dikirim, mis. ['docs/laporan.pdf']"}}},
                  {"caption", {{"type", "string"},
                               {"description", "Keterangan singkat untuk file pertama (opsional)"}}}}},
                {"required", {"paths"}}};
    }

    // Hanya pengiriman yang benar-benar mengunggah dan lolos validasi yang berisiko tinggi.
    // Jika validasi gagal, execute() akan menolak dengan pesan error (tidak ada data keluar),
    // jadi tidak ada gunanya meminta kode authenticator untuk sesuatu yang pasti gagal.
    Risk risk(const nlohmann::json& args) const override {
        if (!sink_.uploads()) return Risk::Safe;
        try {
            (void)plan(args);
            return Risk::Exfiltrate;
        } catch (const std::exception&) {
            return Risk::Safe;
        }
    }

    std::string describe(const nlohmann::json& args) const override {
        try {
            const Plan p = plan(args);
            std::string out = sink_.uploads() ? "Mengunggah ke Telegram (server pihak ketiga):"
                                              : "Menunjukkan lokasi file:";
            for (const auto& it : p.items) {
                out += "\n    - " + it.shown + " (" + human_size(it.size) + ")";
            }
            out += "\n    Total: " + human_size(p.total);

            std::string cap = arg_string(args, "caption", "");
            for (char& c : cap) {
                if (c == '\n') c = ' ';
            }
            if (cap.size() > 100) cap = cap.substr(0, 100) + "...";
            if (!cap.empty()) out += "\n    Keterangan: " + cap;
            return out;
        } catch (const std::exception& e) {
            return std::string("Mengirim file (tidak valid: ") + e.what() + ")";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        // Divalidasi ulang di sini: ukuran dan isi daftar bisa berubah sejak persetujuan.
        const Plan p = plan(args);
        const std::string caption = arg_string(args, "caption", "");

        std::vector<std::string> done;
        for (std::size_t i = 0; i < p.items.size(); ++i) {
            try {
                done.push_back(sink_.deliver(p.items[i].path, i == 0 ? caption : std::string()));
            } catch (const std::exception& e) {
                std::string msg = "Gagal mengirim '" + p.items[i].shown + "': " + e.what();
                if (!done.empty()) msg += " (sudah terkirim: " + join(done) + ")";
                throw std::runtime_error(msg);
            }
        }

        if (sink_.uploads()) {
            return "Terkirim ke Telegram: " + join(done) + ". Isi file tidak dibaca oleh asisten.";
        }
        return "Lokasi file (tidak diunggah): " + join(done) + ".";
    }

private:
    static constexpr std::size_t kMaxFiles = 5;

    struct Item {
        std::filesystem::path path;
        std::string shown;
        std::uintmax_t size = 0;
    };
    struct Plan {
        std::vector<Item> items;
        std::uintmax_t total = 0;
    };

    static std::string join(const std::vector<std::string>& v) {
        std::string out;
        for (std::size_t i = 0; i < v.size(); ++i) out += (i ? "; " : "") + v[i];
        return out;
    }

    // Model kecil kadang mengirim satu string, bukan daftar; keduanya diterima.
    static std::vector<std::string> read_paths(const nlohmann::json& args) {
        if (!args.is_object() || !args.contains("paths")) {
            throw std::runtime_error("Argumen 'paths' wajib diisi (daftar path, maksimal 5).");
        }
        const nlohmann::json& v = args["paths"];
        std::vector<std::string> out;
        if (v.is_string()) {
            out.push_back(v.get<std::string>());
        } else if (v.is_array()) {
            for (const auto& e : v) {
                if (!e.is_string()) throw std::runtime_error("Setiap item 'paths' harus berupa string.");
                out.push_back(e.get<std::string>());
            }
        } else {
            throw std::runtime_error("Argumen 'paths' harus berupa daftar string.");
        }
        if (out.empty()) throw std::runtime_error("'paths' kosong.");
        if (out.size() > kMaxFiles) {
            throw std::runtime_error("Maksimal " + std::to_string(kMaxFiles) + " file per pengiriman.");
        }
        return out;
    }

    std::string shown(const std::filesystem::path& p) const {
        const std::filesystem::path rel = p.lexically_relative(guard_.workspace());
        if (rel.empty() || *rel.begin() == "..") return p.string();
        return rel.string();
    }

    // Seluruh validasi ada di sini. Dipakai risk(), describe(), dan execute().
    Plan plan(const nlohmann::json& args) const {
        namespace fs = std::filesystem;
        Plan p;
        std::set<fs::path> seen;

        for (const std::string& raw : read_paths(args)) {
            const fs::path file = guard_.resolve(raw);  // tolak path di luar allowed_dirs
            if (!seen.insert(file).second) continue;    // duplikat diabaikan

            if (auto why = blocked_reason(file)) {
                throw std::runtime_error("'" + raw + "' tidak boleh dikirim: " + *why + ".");
            }

            std::error_code ec;
            if (!fs::exists(file, ec)) {
                throw std::runtime_error("'" + raw + "' tidak ada. Gunakan search_files atau list_dir "
                                         "untuk mencari path yang benar.");
            }
            if (!fs::is_regular_file(file, ec)) {
                throw std::runtime_error("'" + raw + "' bukan file biasa (folder tidak bisa dikirim).");
            }
            const std::uintmax_t size = fs::file_size(file, ec);
            if (ec) throw std::runtime_error("Gagal membaca ukuran '" + raw + "'.");

            if (sink_.uploads() && size == 0) {
                throw std::runtime_error("'" + raw + "' kosong (0 byte); Telegram menolak file kosong.");
            }
            p.total += size;
            p.items.push_back({file, shown(file), size});
        }

        if (sink_.uploads() && p.total > max_bytes_) {
            throw std::runtime_error("Total ukuran " + human_size(p.total) + " melebihi batas " +
                                     human_size(max_bytes_) + " (config max_send_mb).");
        }
        return p;
    }

    const PathGuard& guard_;
    FileSink& sink_;
    std::uintmax_t max_bytes_;
};
