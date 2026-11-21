#pragma once

#include "utils/config.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace cfgload {

// profiles + active_profile
inline void read_profiles(const nlohmann::json& j, Config& cfg) {
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
}

}  // namespace cfgload