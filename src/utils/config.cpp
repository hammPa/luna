#include "utils/config.hpp"
#include "utils/log.hpp"

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
        for (const char* old : {"backend", "model", "base_url", "api_key_env",
                                "max_context", "timeout_sec", "history_limit"}) {
            if (j.contains(old)) {
                throw std::runtime_error(std::string("field '") + old +
                    "' tingkat atas tidak dipakai lagi sejak 1.5; pindahkan ke 'providers' "
                    "(lihat config.example.json)");
            }
        }
        cfg.system_prompt = j.value("system_prompt", cfg.system_prompt);
        cfg.memory_db = j.value("memory_db", cfg.memory_db);

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

        if (j.contains("profiles")) {
            const auto& pr = j.at("profiles");
            if (!pr.is_object()) throw std::runtime_error("'profiles' harus object");
            for (const auto& [name, v] : pr.items()) {
                if (!v.is_array()) {
                    throw std::runtime_error("profil '" + name + "' harus array nama tool");
                }
                cfg.profiles[name] = v.get<std::vector<std::string>>();
            }
        }
        cfg.active_profile = j.value("active_profile", cfg.active_profile);
        if (!cfg.active_profile.empty() && cfg.profiles.count(cfg.active_profile) == 0) {
            throw std::runtime_error("active_profile '" + cfg.active_profile +
                                     "' tidak ada di profiles");
        }

        cfg.telegram_token_env = j.value("telegram_token_env", cfg.telegram_token_env);
        cfg.telegram_api_base_url = j.value("telegram_api_base_url", cfg.telegram_api_base_url);
        if (j.contains("telegram_allowed_users")) {
            cfg.telegram_allowed_users = j.at("telegram_allowed_users").get<std::vector<long long>>();
        }

        // Provider + model aktif. Provider: backend, base_url, api_key_env, timeout_sec.
        // Model: max_context, history_limit.
        if (!j.contains("providers") || !j.at("providers").is_object() ||
            j.at("providers").empty()) {
            throw std::runtime_error("'providers' wajib (object, minimal satu provider); "
                                     "lihat config.example.json");
        }
        if (!j.contains("active") || !j.at("active").is_object()) {
            throw std::runtime_error("'active' wajib: {\"provider\": \"...\", \"model\": \"...\"}");
        }
        const std::string act_prov  = j.at("active").value("provider", "");
        const std::string act_model = j.at("active").value("model", "");
        if (act_prov.empty() || act_model.empty()) {
            throw std::runtime_error("'active' harus berisi 'provider' dan 'model'");
        }

        const auto& provs = j.at("providers");
        if (!provs.contains(act_prov) || !provs.at(act_prov).is_object()) {
            throw std::runtime_error("active.provider '" + act_prov + "' tidak ada di providers");
        }
        const auto& p = provs.at(act_prov);
        if (!p.contains("models") || !p.at("models").is_object() ||
            !p.at("models").contains(act_model) || !p.at("models").at(act_model).is_object()) {
            throw std::runtime_error("model '" + act_model + "' tidak terdaftar di providers." +
                                     act_prov + ".models (isi max_context dan history_limit)");
        }
        const auto& m = p.at("models").at(act_model);

        for (const char* key : {"backend", "base_url", "api_key_env", "timeout_sec"}) {
            if (!p.contains(key)) {
                throw std::runtime_error("providers." + act_prov + " wajib punya '" + key + "'");
            }
        }
        for (const char* key : {"max_context", "history_limit"}) {
            if (!m.contains(key)) {
                throw std::runtime_error("model '" + act_model + "' wajib punya '" + key + "'");
            }
        }

        cfg.active_provider = act_prov;
        cfg.model           = act_model;
        cfg.backend         = p.at("backend").get<std::string>();
        cfg.base_url        = p.at("base_url").get<std::string>();
        cfg.api_key_env     = p.at("api_key_env").get<std::string>();
        cfg.timeout_sec     = p.at("timeout_sec").get<long>();
        cfg.max_context     = m.at("max_context").get<int>();

        const int hl = m.at("history_limit").get<int>();
        if (hl < 1) {
            throw std::runtime_error("history_limit harus >= 1");
        }
        cfg.history_limit = static_cast<std::size_t>(hl);

        cfg.max_send_mb = j.value("max_send_mb", cfg.max_send_mb);
        if (cfg.max_send_mb < 1 || cfg.max_send_mb > 50) {
            throw std::runtime_error("max_send_mb harus 1 sampai 50 (batas bot Telegram 50 MB)");
        }

        if (j.contains("search")) {
            const auto& s = j.at("search");
            if (!s.is_object()) throw std::runtime_error("'search' harus object");
            cfg.search_provider    = s.value("provider", cfg.search_provider);
            cfg.search_api_key_env = s.value("api_key_env", cfg.search_api_key_env);
            cfg.search_base_url    = s.value("base_url", cfg.search_base_url);
            cfg.search_max_results = s.value("max_results", cfg.search_max_results);
            if (cfg.search_max_results < 1 || cfg.search_max_results > 10) {
                throw std::runtime_error("search.max_results harus 1 sampai 10");
            }
        }
        cfg.default_location = j.value("default_location", cfg.default_location);

        cfg.log_file  = j.value("log_file", cfg.log_file);
        cfg.log_level = j.value("log_level", cfg.log_level);
        (void)parse_log_level(cfg.log_level);   // validasi; melempar runtime_error jika salah
        cfg.log_max_kb = j.value("log_max_kb", cfg.log_max_kb);
        if (cfg.log_max_kb < 64 || cfg.log_max_kb > 102400) {
            throw std::runtime_error("log_max_kb harus 64 sampai 102400");
        }

        cfg.input_history_max = j.value("input_history_max", cfg.input_history_max);
        if (cfg.input_history_max < 0 || cfg.input_history_max > 10000) {
            throw std::runtime_error("input_history_max harus 0 sampai 10000");
        }
    } catch (const json::exception& e) {
        // misalnya "max_context" berisi string, bukan angka
        throw std::runtime_error("Tipe field config salah: " + std::string(e.what()));
    }

    if (cfg.model.empty()) {
        throw std::runtime_error("Field 'model' wajib diisi di config");
    }
    return cfg;
}