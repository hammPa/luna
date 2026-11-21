#pragma once

#include "tools/tool.hpp"
#include "tools/permission.hpp"

#include <map>
#include <memory>
#include <set>
#include <vector>
#include <string>

struct ToolStatus {
    std::string name;
    bool enabled;
};

class ToolRegistry {
public:
    explicit ToolRegistry(PermissionGate& gate);

    // Registry memegang kepemilikan tool. Lempar exception kalau nama sudah ada.
    void add(std::unique_ptr<Tool> tool);

    // Schema semua tool dalam format "internal" (bukan format provider).
    // Penerjemahan ke format OpenAI dilakukan di openai_backend.cpp.
    nlohmann::json schema() const;

    // Jalankan tool by nama. TIDAK PERNAH melempar: semua kegagalan
    // dikembalikan sebagai teks supaya bisa dikirim balik ke model.
    std::string run(const std::string& name, const nlohmann::json& args);

    bool empty() const { return tools_.empty(); }

    // Ringkasan "- nama: deskripsi" per tool, untuk system prompt.
    std::string describe_all() const;

    // Daftar semua tool terdaftar (urut nama) beserta status aktifnya.
    std::vector<ToolStatus> list() const;

    // Hidup/matikan tool. false = nama tidak terdaftar.
    bool set_enabled(const std::string& name, bool enabled);
    
    // Profil = kumpulan tool aktif. Dipanggil sekali saat start.
    void set_profiles(std::map<std::string, std::vector<std::string>> profiles);

    // Hanya tool di profil yang aktif ("*" = semua). false = profil tidak ada.
    // `unknown` diisi nama di profil yang tidak terdaftar.
    bool use_profile(const std::string& name, std::vector<std::string>& unknown);

    const std::map<std::string, std::vector<std::string>>& profiles() const { return profiles_; }
    const std::string& active_profile() const { return active_profile_; }
private:
    PermissionGate& gate_;
    std::map<std::string, std::unique_ptr<Tool>> tools_;
    std::set<std::string> disabled_;
    std::map<std::string, std::vector<std::string>> profiles_;
    std::string active_profile_;
};