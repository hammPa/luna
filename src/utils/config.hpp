#pragma once

#include <cstddef>
#include <string>
#include <map>
#include <vector>

struct AppEntry {
    std::string run;       // program untuk membuka
    std::string process;   // nama proses untuk menutup (kosong = tidak bisa ditutup)
};

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
    std::map<std::string, AppEntry> apps;  // nama app -> entry

    std::vector<long long> telegram_allowed_users;
    std::string telegram_token_env = "LUNA_ASSISTANT_BOT_TOKEN";
    std::string telegram_api_base_url = "https://api.telegram.org";
};

// Lempar std::runtime_error jika file tidak ada, JSON rusak, atau field tidak valid.
Config load_config(const std::string& path);