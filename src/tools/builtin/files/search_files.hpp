#pragma once

// Lewati file ini jika kamu sudah punya SearchFilesTool dari paket tool sebelumnya
// (file_tools.hpp); nama class-nya sama dan akan bentrok.

#include "tools/file_blocklist.hpp"
#include "tools/path_guard.hpp"  // PathGuard, arg_string
#include "tools/tool.hpp"
#include "utils/format_utils.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

class SearchFilesTool : public Tool {
public:
    explicit SearchFilesTool(const PathGuard& g) : guard_(g) {}

    std::string name() const override { return "search_files"; }

    std::string description() const override {
        return "Mencari file atau folder berdasarkan potongan nama (tidak peka huruf besar/kecil) di "
               "workspace, termasuk subfolder. Mengembalikan path relatif beserta ukuran, yang bisa "
               "dipakai read_file atau send_file. Maksimal 100 hasil. File rahasia tidak ditampilkan.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"pattern", {{"type", "string"},
                               {"description", "Potongan nama, mis. 'laporan' atau '.pdf'"}}},
                  {"path", {{"type", "string"},
                            {"description", "Folder awal pencarian, default '.'"}}}}},
                {"required", {"pattern"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            return "Mencari file: " + arg_string(args, "pattern", "?");
        } catch (const std::exception&) {
            return "Mencari file (argumen tidak valid)";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        namespace fs = std::filesystem;
        const std::string pat = lower(arg_string(args, "pattern", ""));
        if (pat.empty()) throw std::runtime_error("Pola pencarian kosong.");
        const fs::path root = guard_.resolve(arg_string(args, "path", "."));

        std::error_code ec;
        if (!fs::is_directory(root, ec)) throw std::runtime_error("Folder awal tidak ada.");

        const fs::path trash = guard_.workspace() / ".trash";
        std::vector<std::string> hits;
        std::size_t scanned = 0;
        bool cut = false;

        // Symlink folder tidak diikuti (default), jadi pencarian tidak bisa keluar dari workspace.
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            if (++scanned > kMaxScan) { cut = true; break; }

            std::error_code e2;
            const bool is_dir = it->is_directory(e2);

            if (it->path() == trash || (is_dir && blocked_reason(it->path()))) {
                it.disable_recursion_pending();  // jangan masuk .trash atau folder rahasia
                continue;
            }
            if (blocked_reason(it->path())) continue;  // file rahasia tidak ditampilkan

            if (lower(it->path().filename().string()).find(pat) == std::string::npos) continue;

            std::string line = shown(it->path());
            if (is_dir) {
                line += "/";
            } else {
                const auto size = it->file_size(e2);
                if (!e2) line += " (" + human_size(size) + ")";
            }
            hits.push_back(std::move(line));
            if (hits.size() >= kMaxHits) { cut = true; break; }
        }

        if (hits.empty()) return cut ? "Tidak ketemu (pencarian dibatasi)." : "Tidak ada yang cocok.";

        std::sort(hits.begin(), hits.end());
        std::string out;
        for (const auto& h : hits) out += h + "\n";
        if (cut) out += "[hasil dipotong, persempit pola atau folder]\n";
        return out;
    }

private:
    static constexpr std::size_t kMaxHits = 100;
    static constexpr std::size_t kMaxScan = 20000;

    static std::string lower(std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::string shown(const std::filesystem::path& p) const {
        const std::filesystem::path rel = p.lexically_relative(guard_.workspace());
        if (rel.empty() || *rel.begin() == "..") return p.string();
        return rel.string();
    }

    const PathGuard& guard_;
};
