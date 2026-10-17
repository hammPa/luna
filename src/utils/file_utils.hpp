#pragma once

#include "tools/path_guard.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

constexpr std::size_t kMaxWriteBytes = 256 * 1024;

// Ambil argumen "content"/"old"/"new" (boleh string kosong).
// Pakai arg_string dari path_guard.hpp.

// Tulis atomik: file sementara di folder yang sama, lalu rename.
inline void write_file_atomic(const std::filesystem::path& target,
                              const std::string& content) {
    namespace fs = std::filesystem;
    if (content.size() > kMaxWriteBytes) {
        throw std::runtime_error("Konten terlalu besar (maks " +
                                 std::to_string(kMaxWriteBytes) + " byte).");
    }

    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    if (ec) throw std::runtime_error("Gagal membuat folder: " + ec.message());

    fs::path tmp = target;
    tmp += ".luna-tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Gagal membuka file untuk ditulis.");
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out) {
            fs::remove(tmp, ec);
            throw std::runtime_error("Gagal menulis file.");
        }
    }
    fs::rename(tmp, target, ec);  // atomik di filesystem yang sama
    if (ec) {
        fs::remove(tmp, ec);
        throw std::runtime_error("Gagal memindahkan file: " + ec.message());
    }
}

// Potong teks panjang untuk ditampilkan di prompt izin.
inline std::string preview(const std::string& s, std::size_t max = 120) {
    std::string t = s.size() > max ? s.substr(0, max) + "..." : s;
    for (char& c : t) if (c == '\n') c = ' ';
    return t;
}