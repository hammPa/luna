#include "utils/config.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

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
        cfg.backend       = j.value("backend", cfg.backend);
        cfg.model         = j.value("model", cfg.model);
        cfg.base_url      = j.value("base_url", cfg.base_url);
        cfg.api_key_env   = j.value("api_key_env", cfg.api_key_env);
        cfg.system_prompt = j.value("system_prompt", cfg.system_prompt);
        cfg.max_context   = j.value("max_context", cfg.max_context);
        cfg.timeout_sec   = j.value("timeout_sec", cfg.timeout_sec);
        cfg.memory_db = j.value("memory_db", cfg.memory_db);

        int limit = j.value("history_limit", static_cast<int>(cfg.history_limit));
        if (limit < 1) {
            throw std::runtime_error("history_limit harus >= 1");
        }
        
        if (j.contains("allowed_dirs")) {
            cfg.allowed_dirs = j.at("allowed_dirs").get<std::vector<std::string>>();
        }
        if (j.contains("permissions")) {
            cfg.permissions = j.at("permissions").get<std::map<std::string, std::string>>();
        }
        if (j.contains("apps")) {
            for (const auto& [name, v] : j.at("apps").items()) {
                AppEntry e;
                if (v.is_string()) {            // format lama tetap diterima
                    e.run = v.get<std::string>();
                } else {
                    e.run = v.value("run", "");
                    e.process = v.value("process", "");
                }
                cfg.apps[name] = std::move(e);
            }
        }

        cfg.telegram_token_env = j.value("telegram_token_env", cfg.telegram_token_env);
        if (j.contains("telegram_allowed_users")) {
            cfg.telegram_allowed_users = j.at("telegram_allowed_users").get<std::vector<long long>>();
        }
        
        cfg.history_limit = static_cast<std::size_t>(limit);
    } catch (const json::exception& e) {
        // misalnya "model" berisi angka, bukan string
        throw std::runtime_error("Tipe field config salah: " + std::string(e.what()));
    }

    if (cfg.model.empty()) {
        throw std::runtime_error("Field 'model' wajib diisi di config");
    }
    return cfg;
}