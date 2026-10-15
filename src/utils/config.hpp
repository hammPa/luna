#pragma once

#include <cstddef>
#include <string>
#include <map>
#include <vector>

struct Config {
    std::string backend = "openai";
    std::string model;
    std::string base_url;
    std::string api_key_env;
    std::string system_prompt = "Kamu asisten pribadi.";
    std::size_t history_limit = 20;
    int max_context = 4096;  // belum dipakai di Fase 1, disiapkan untuk backend lokal
    long timeout_sec = 30;
    std::string memory_db = "memory.db";
    std::vector<std::string> allowed_dirs;
    std::map<std::string, std::string> permissions;  // "write" -> "ask"
};

// Lempar std::runtime_error jika file tidak ada, JSON rusak, atau field tidak valid.
Config load_config(const std::string& path);