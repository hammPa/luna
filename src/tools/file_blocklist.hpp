#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>

// File yang TIDAK BOLEH dikirim keluar dan tidak ditampilkan di hasil pencarian,
// walaupun berada di dalam allowed_dirs. Jaring terakhir kalau ada rahasia yang
// tidak sengaja tersimpan di workspace.
//
// Pemeriksaan dilakukan pada path yang sudah dinormalisasi (symlink diselesaikan
// PathGuard), jadi symlink bernama "foto.jpg" yang menunjuk ke ".ssh/id_rsa" tetap kena.
// Mengembalikan alasan penolakan, atau std::nullopt jika boleh.
inline std::optional<std::string> blocked_reason(const std::filesystem::path& p) {
    auto lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    auto starts_with = [](const std::string& s, const std::string& pre) {
        return s.size() >= pre.size() && s.compare(0, pre.size(), pre) == 0;
    };
    auto ends_with = [](const std::string& s, const std::string& suf) {
        return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
    };

    static const char* const kDirs[] = {".ssh", ".gnupg", ".aws", ".kube", ".git"};
    static const char* const kNames[] = {".env", ".netrc", ".npmrc", ".pypirc", "config.json",
                                         "authorized_keys", "known_hosts", "credentials",
                                         "credentials.json", "secrets.json"};
    static const char* const kPrefixes[] = {".env.", "id_rsa", "id_dsa", "id_ecdsa", "id_ed25519"};
    static const char* const kSuffixes[] = {".pem", ".key", ".p12", ".pfx", ".kdbx", ".db",
                                            ".sqlite", ".sqlite3", ".db-wal", ".db-shm",
                                            ".db-journal", ".luna-tmp"};

    for (const auto& part : p) {
        const std::string c = lower(part.string());
        for (const char* d : kDirs) {
            if (c == d) return "berada di folder rahasia (" + std::string(d) + ")";
        }
    }

    const std::string name = lower(p.filename().string());
    for (const char* n : kNames) {
        if (name == n) return "nama file termasuk daftar rahasia (" + name + ")";
    }
    for (const char* pre : kPrefixes) {
        if (starts_with(name, pre)) return "nama file termasuk daftar rahasia (" + name + ")";
    }
    for (const char* suf : kSuffixes) {
        if (ends_with(name, suf)) return "jenis file termasuk daftar rahasia (" + std::string(suf) + ")";
    }
    return std::nullopt;
}
