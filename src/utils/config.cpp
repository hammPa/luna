#include "utils/config.hpp"
#include "utils/log.hpp"

#include "utils/config/apps.hpp"
#include "utils/config/misc.hpp"
#include "utils/config/profiles.hpp"
#include "utils/config/providers.hpp"
#include "utils/config/reject_old.hpp"
#include "utils/config/search.hpp"
#include "utils/config/telegram.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

void apply_model(Config& cfg, std::size_t index) {
    const ModelChoice& c = cfg.models.at(index);
    const ProviderEntry& p = cfg.providers.at(c.provider);

    cfg.active_index    = index;
    cfg.active_provider = c.provider;
    cfg.model           = c.model;
    cfg.backend         = p.backend;
    cfg.base_url        = p.base_url;
    cfg.api_key_env     = p.api_key_env;
    cfg.timeout_sec     = p.timeout_sec;
    cfg.max_context     = c.max_context;
    cfg.history_limit   = c.history_limit;
}

Config load_config(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Tidak bisa membuka config: " + path);
    }

    json j;
    try {
        j = json::parse(file);
    } catch (const json::exception& e) {
        throw std::runtime_error("config.json bukan JSON valid: " + std::string(e.what()));
    }

    Config cfg;
    try {
        cfgload::reject_old_fields(j);
        cfg.system_prompt = j.value("system_prompt", cfg.system_prompt);
        cfg.memory_db = j.value("memory_db", cfg.memory_db);

        if (j.contains("allowed_dirs")) {
            cfg.allowed_dirs = j.at("allowed_dirs").get<std::vector<std::string>>();
        }
        if (j.contains("permissions")) {
            cfg.permissions = j.at("permissions").get<std::map<std::string, std::string>>();
        }
        cfgload::read_apps(j, cfg);
        cfgload::read_profiles(j, cfg);
        cfgload::read_telegram(j, cfg);
        cfgload::read_providers(j, cfg);

        cfg.max_send_mb = j.value("max_send_mb", cfg.max_send_mb);
        if (cfg.max_send_mb < 1 || cfg.max_send_mb > 50) {
            throw std::runtime_error("max_send_mb harus 1 sampai 50 (batas bot Telegram 50 MB)");
        }

        cfgload::read_search(j, cfg);
        cfgload::read_misc(j, cfg);
    } catch (const json::exception& e) {
        // misalnya "max_context" berisi string, bukan angka
        throw std::runtime_error("Tipe field config salah: " + std::string(e.what()));
    }

    if (cfg.model.empty()) {
        throw std::runtime_error("Field 'model' wajib diisi di config");
    }
    return cfg;
}