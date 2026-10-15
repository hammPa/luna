#pragma once

#include "tools/tool.hpp"
#include "tools/permission.hpp"

#include <map>
#include <memory>
#include <string>

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
    
private:
    PermissionGate& gate_;
    std::map<std::string, std::unique_ptr<Tool>> tools_;
};