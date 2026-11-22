#pragma once

// Bantu HTTP untuk OpenAIBackend: pesan error server dan aturan retry rate limit.

#include "llm/llm_types.hpp"
#include "utils/http.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

namespace openai {

using json = nlohmann::json;

// Pesan error dari server. Format Mistral: {"message": "..."}
// Format OpenAI: {"error": {"message": "..."}}
inline std::string server_message(const std::string& body) {
    try {
        json j = json::parse(body);
        if (j.contains("error") && j["error"].is_object() &&
            j["error"].contains("message") && j["error"]["message"].is_string()) {
            return j["error"]["message"].get<std::string>();
        }
        if (j.contains("message") && j["message"].is_string()) {
            return j["message"].get<std::string>();
        }
    } catch (const json::exception&) {
        // body bukan JSON, abaikan
    }
    return body;
}

inline std::string describe_http_error(const HttpResponse& res) {
    std::string detail = server_message(res.body);
    switch (res.statusCode) {
        case 401: return "API key salah atau tidak valid (401): " + detail;
        case 429: return "Kena rate limit (429), coba lagi sebentar: " + detail;
        default:  return "HTTP " + std::to_string(res.statusCode) + ": " + detail;
    }
}

constexpr int kMaxRetries = 2;
constexpr double kMaxWaitSec = 20.0;

// 429, atau 503 dari 9router yang membungkus 429 milik Groq.
inline bool is_rate_limited(const HttpResponse& res) {
    if (res.statusCode == 429) return true;
    if (res.statusCode != 503) return false;
    std::string b = res.body;
    std::transform(b.begin(), b.end(), b.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return b.find("429") != std::string::npos ||
           b.find("rate limit") != std::string::npos ||
           b.find("rate_limit") != std::string::npos ||
           b.find("try again in") != std::string::npos;
}

// Baca "try again in 6.57s" / "1m30.5s" / "250ms". Hasil detik, -1 kalau tidak ada.
inline double parse_retry_after(const std::string& msg) {
    const std::string key = "try again in ";
    const std::size_t pos = msg.find(key);
    if (pos == std::string::npos) return -1;
    const char* p = msg.c_str() + pos + key.size();
    double total = 0;
    bool any = false;
    while (true) {
        char* end = nullptr;
        const double v = std::strtod(p, &end);
        if (end == p) break;
        if (end[0] == 'm' && end[1] == 's') { total += v / 1000.0; p = end + 2; }
        else if (end[0] == 'm')             { total += v * 60.0;   p = end + 1; }
        else if (end[0] == 's')             { total += v;          p = end + 1; }
        else break;
        any = true;
    }
    return any ? total : -1;
}

// Tidur per 100 ms supaya Esc/Ctrl+C tetap terasa. false = dibatalkan.
inline bool sleep_cancellable(double sec, const std::atomic<bool>* cancel) {
    const auto end = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(static_cast<long long>(sec * 1000));
    while (std::chrono::steady_clock::now() < end) {
        if (cancel && cancel->load()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return !(cancel && cancel->load());
}

}  // namespace openai