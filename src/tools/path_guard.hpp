#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>


// Satu-satunya penjaga path untuk semua tool file.
// Path relatif diartikan relatif terhadap allowed_dirs[0] (workspace utama).
class PathGuard {
public:
    // Lempar runtime_error kalau allowed_dirs kosong.
    // Folder yang belum ada akan dibuat.
    explicit PathGuard(const std::vector<std::string>& allowed_dirs);

    // Mengembalikan path absolut yang sah.
    // Lempar runtime_error dengan pesan yang mengarahkan model kalau ditolak.
    std::filesystem::path resolve(const std::string& user_path) const;

    const std::filesystem::path& workspace() const { return allowed_.front(); }

private:
    bool is_inside(const std::filesystem::path& p) const;

    std::vector<std::filesystem::path> allowed_;  // sudah canonical
};

// Ambil argumen string dari JSON. Lempar runtime_error yang jelas kalau tipenya salah.
inline std::string arg_string(const nlohmann::json& args, const char* key,
                              const std::string& fallback) {
    if (!args.is_object() || !args.contains(key) || args[key].is_null()) {
        return fallback;
    }
    if (!args[key].is_string()) {
        throw std::runtime_error(std::string("Argumen '") + key + "' harus berupa string.");
    }
    return args[key].get<std::string>();
}