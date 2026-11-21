#pragma once

#include "utils/config.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace cfgload {

inline void read_apps(const nlohmann::json& j, Config& cfg) {
    if (!j.contains("apps")) return;
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

}  // namespace cfgload