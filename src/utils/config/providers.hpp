#pragma once

#include "utils/config.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

namespace cfgload {

// Provider: backend, base_url, api_key_env, timeout_sec, models.
// Model: max_context, history_limit.
// Semua provider dan model divalidasi saat start, supaya /model aman dipakai.
// Memanggil apply_model() untuk pasangan aktif.
inline void read_providers(const nlohmann::json& j, Config& cfg) {
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
    for (const auto& [pname, pv] : provs.items()) {
        if (!pv.is_object()) {
            throw std::runtime_error("providers." + pname + " harus object");
        }
        for (const char* key : {"backend", "base_url", "api_key_env", "timeout_sec", "models"}) {
            if (!pv.contains(key)) {
                throw std::runtime_error("providers." + pname + " wajib punya '" + key + "'");
            }
        }
        if (!pv.at("models").is_object()) {
            throw std::runtime_error("providers." + pname + ".models harus object");
        }

        ProviderEntry pe;
        pe.backend     = pv.at("backend").get<std::string>();
        pe.base_url    = pv.at("base_url").get<std::string>();
        pe.api_key_env = pv.at("api_key_env").get<std::string>();
        pe.timeout_sec = pv.at("timeout_sec").get<long>();
        cfg.providers[pname] = std::move(pe);

        for (const auto& [mname, mv] : pv.at("models").items()) {
            if (!mv.is_object()) {
                throw std::runtime_error("model '" + mname + "' harus object "
                                         "(isi max_context dan history_limit)");
            }
            for (const char* key : {"max_context", "history_limit"}) {
                if (!mv.contains(key)) {
                    throw std::runtime_error("model '" + mname + "' wajib punya '" + key + "'");
                }
            }
            ModelChoice mc;
            mc.provider    = pname;
            mc.model       = mname;
            mc.max_context = mv.at("max_context").get<int>();

            const int hl = mv.at("history_limit").get<int>();
            if (hl < 1) {
                throw std::runtime_error("history_limit harus >= 1");
            }
            mc.history_limit = static_cast<std::size_t>(hl);
            cfg.models.push_back(std::move(mc));
        }
    }

    if (cfg.providers.count(act_prov) == 0) {
        throw std::runtime_error("active.provider '" + act_prov + "' tidak ada di providers");
    }
    bool found = false;
    for (std::size_t i = 0; i < cfg.models.size(); ++i) {
        if (cfg.models[i].provider == act_prov && cfg.models[i].model == act_model) {
            apply_model(cfg, i);
            found = true;
            break;
        }
    }
    if (!found) {
        throw std::runtime_error("model '" + act_model + "' tidak terdaftar di providers." +
                                 act_prov + ".models (isi max_context dan history_limit)");
    }
}

}  // namespace cfgload