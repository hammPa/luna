#pragma once

#include <string>
#include <vector>

struct SlashCmd {
    std::string name;
    std::string desc;
    bool takes_arg;
};

// Samakan daftar ini dengan perintah yang dikenal handle_command.
inline const std::vector<SlashCmd>& slash_commands() {
    static const std::vector<SlashCmd> v = {
        {"/remember", "simpan fakta: /remember kunci=nilai", true},
        {"/facts",    "tampilkan fakta tersimpan", false},
        {"/clear",    "hapus riwayat chat (fakta tetap)", false},
        {"/help",     "bantuan", false},
        {"/ctx",      "ukuran konteks yang terpakai", false},
        {"/tools",    "daftar tool dan statusnya", false},
        {"/tool",     "hidup/matikan tool: /tool on|off nama", true},
        {"/profile",  "ganti profil tool: /profile nama", true},
        {"/model",    "ganti model: /model nomor", true},
        {"/copy",     "salin balasan terakhir (/copy code = blok kode)", false},
        {"/exit",     "keluar dari Luna", false},
    };
    return v;
}

// Kosong jika input bukan perintah, atau sudah ada spasi (sedang mengetik argumen).
inline std::vector<SlashCmd> slash_matches(const std::string& in) {
    std::vector<SlashCmd> out;
    if (in.empty() || in[0] != '/' || in.find(' ') != std::string::npos) return out;
    for (const auto& c : slash_commands()) {
        if (c.name.compare(0, in.size(), in) == 0) out.push_back(c);
    }
    return out;
}