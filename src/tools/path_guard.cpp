#include "tools/path_guard.hpp"

#include <stdexcept>

namespace fs = std::filesystem;

PathGuard::PathGuard(const std::vector<std::string>& allowed_dirs) {
    if (allowed_dirs.empty()) {
        throw std::runtime_error("PathGuard: allowed_dirs kosong");
    }
    for (const auto& d : allowed_dirs) {
        if (d.empty()) {
            throw std::runtime_error("PathGuard: ada allowed_dir yang kosong");
        }
        std::error_code ec;
        fs::create_directories(d, ec);  // sudah ada = bukan error
        if (ec) {
            throw std::runtime_error("PathGuard: gagal membuat folder '" + d +
                                     "': " + ec.message());
        }
        // canonical menyelesaikan symlink dan "..", folder pasti ada di sini.
        allowed_.push_back(fs::canonical(d));
    }
}

// Cek per KOMPONEN path, bukan awalan string.
// "/a/b-evil" tidak dianggap berada di dalam "/a/b".
bool PathGuard::is_inside(const fs::path& p) const {
    for (const auto& base : allowed_) {
        auto bi = base.begin();
        auto pi = p.begin();
        for (; bi != base.end() && pi != p.end(); ++bi, ++pi) {
            if (*bi != *pi) break;
        }
        if (bi == base.end()) return true;  // semua komponen base cocok
    }
    return false;
}

fs::path PathGuard::resolve(const std::string& user_path) const {
    if (user_path.empty()) {
        throw std::runtime_error(
            "Path kosong. Berikan path, misalnya 'catatan.txt', atau '.' untuk workspace.");
    }
    if (user_path.find('\0') != std::string::npos) {
        throw std::runtime_error("Path mengandung karakter tidak valid.");
    }

    fs::path p(user_path);
    if (p.is_relative()) {
        p = allowed_.front() / p;
    }

    // weakly_canonical: selesaikan "..", ".", symlink pada bagian yang ada,
    // tetap jalan untuk file yang belum ada.
    std::error_code ec;
    fs::path resolved = fs::weakly_canonical(p, ec);
    if (ec) {
        throw std::runtime_error("Path tidak valid: " + ec.message());
    }

    if (!is_inside(resolved)) {
        throw std::runtime_error(
            "Path di luar folder yang diizinkan. Gunakan list_dir dengan path '.' "
            "untuk melihat isi workspace, dan pakai path relatif.");
    }
    return resolved;
}