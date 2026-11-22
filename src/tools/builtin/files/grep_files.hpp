#pragma once

#include "tools/file_blocklist.hpp"
#include "tools/path_guard.hpp"  // PathGuard, arg_string
#include "tools/tool.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

class GrepFilesTool : public Tool {
public:
    explicit GrepFilesTool(const PathGuard& g) : guard_(g) {}

    std::string name() const override { return "grep_files"; }

    std::string description() const override {
        return "Mencari TEKS di dalam isi file teks di workspace, termasuk subfolder (tidak peka "
               "huruf besar/kecil, teks biasa, bukan regex). Mengembalikan 'path:nomor_baris: isi "
               "baris'. Maksimal 50 hasil. File biner, file di atas 1 MB, dan file rahasia dilewati. "
               "Untuk mencari NAMA file pakai search_files.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties",
                 {{"pattern", {{"type", "string"},
                               {"description", "Teks yang dicari, mis. 'pajak'"}}},
                  {"path", {{"type", "string"},
                            {"description", "Folder atau file awal pencarian, default '.'"}}}}},
                {"required", {"pattern"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            return "Mencari teks: " + arg_string(args, "pattern", "?");
        } catch (const std::exception&) {
            return "Mencari teks (argumen tidak valid)";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        namespace fs = std::filesystem;
        const std::string raw = arg_string(args, "pattern", "");
        if (raw.empty()) throw std::runtime_error("Teks yang dicari kosong.");
        if (raw.size() > kMaxPattern) throw std::runtime_error("Teks yang dicari terlalu panjang.");
        const std::string pat = lower(raw);
        const fs::path root = guard_.resolve(arg_string(args, "path", "."));

        std::error_code ec;
        std::vector<fs::path> files;
        bool cut = false;

        // Tahap 1: kumpulkan file yang boleh dibaca.
        if (fs::is_regular_file(root, ec)) {
            if (blocked_reason(root)) throw std::runtime_error("File ini termasuk daftar rahasia.");
            if (fs::file_size(root, ec) > kMaxFileBytes) {
                throw std::runtime_error("File terlalu besar (maks 1 MB).");
            }
            files.push_back(root);
        } else if (fs::is_directory(root, ec)) {
            const fs::path trash = guard_.workspace() / ".trash";
            std::size_t scanned = 0;
            fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
            const fs::recursive_directory_iterator end;
            for (; !ec && it != end; it.increment(ec)) {
                if (++scanned > kMaxScan) { cut = true; break; }

                std::error_code e2;
                // Symlink dilewati: isinya bisa menunjuk ke luar workspace.
                if (it->is_symlink(e2)) continue;

                const bool is_dir = it->is_directory(e2);
                if (it->path() == trash || (is_dir && blocked_reason(it->path()))) {
                    it.disable_recursion_pending();  // jangan masuk .trash atau folder rahasia
                    continue;
                }
                if (is_dir || !it->is_regular_file(e2)) continue;
                if (blocked_reason(it->path())) continue;

                const auto size = it->file_size(e2);
                if (e2 || size > kMaxFileBytes) continue;

                if (files.size() >= kMaxFiles) { cut = true; break; }
                files.push_back(it->path());
            }
        } else {
            throw std::runtime_error("Path tidak ada.");
        }

        // Tahap 2: baca dan cocokkan. Diurutkan supaya hasil stabil.
        std::sort(files.begin(), files.end());
        std::vector<std::string> hits;
        std::size_t total = 0;

        for (const auto& f : files) {
            if (hits.size() >= kMaxHits) { cut = true; break; }

            std::ifstream in(f, std::ios::binary);
            if (!in) continue;
            const std::string data((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
            total += data.size();
            if (data.find('\0') != std::string::npos) continue;  // biner

            std::size_t pos = 0;
            std::size_t lineno = 0;
            while (pos <= data.size()) {
                const std::size_t nl = data.find('\n', pos);
                const bool last = (nl == std::string::npos);
                std::string line = data.substr(pos, last ? std::string::npos : nl - pos);
                ++lineno;
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (lower(line).find(pat) != std::string::npos) {
                    hits.push_back(shown(f) + ":" + std::to_string(lineno) + ": " + clip(line));
                    if (hits.size() >= kMaxHits) { cut = true; break; }
                }
                if (last) break;
                pos = nl + 1;
            }
            if (total > kMaxTotalBytes) { cut = true; break; }
        }

        if (hits.empty()) return cut ? "Tidak ketemu (pencarian dibatasi)." : "Tidak ada yang cocok.";

        std::string out;
        for (const auto& h : hits) out += h + "\n";
        if (cut) out += "[hasil dipotong, persempit teks atau folder]\n";
        return out;
    }

private:
    static constexpr std::size_t kMaxPattern = 200;
    static constexpr std::size_t kMaxHits = 50;
    static constexpr std::size_t kMaxLine = 200;             // byte per baris hasil
    static constexpr std::size_t kMaxScan = 20000;           // entri folder yang ditelusuri
    static constexpr std::size_t kMaxFiles = 2000;           // file yang dibaca
    static constexpr std::uintmax_t kMaxFileBytes = 1024 * 1024;
    static constexpr std::size_t kMaxTotalBytes = 32 * 1024 * 1024;

    static std::string lower(std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    // Potong di batas karakter UTF-8, jangan di tengah byte lanjutan.
    static std::string clip(const std::string& s) {
        if (s.size() <= kMaxLine) return s;
        std::size_t n = kMaxLine;
        while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
        return s.substr(0, n) + "...";
    }

    std::string shown(const std::filesystem::path& p) const {
        const std::filesystem::path rel = p.lexically_relative(guard_.workspace());
        if (rel.empty() || *rel.begin() == "..") return p.string();
        return rel.string();
    }

    const PathGuard& guard_;
};