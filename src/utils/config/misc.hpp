#pragma once

#include "utils/config.hpp"
#include "utils/log.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace cfgload {

// default_location, log_*, input_history_max
inline void read_misc(const nlohmann::json& j, Config& cfg) {
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
}

}  // namespace cfgload