#pragma once
#include <cstdio>
#include <exception>
#include <string>
#include <utility>
#include <nlohmann/json.hpp>

#include "tools/tool.hpp"
#include "utils/http.hpp"

class WeatherTool : public Tool {
public:
    WeatherTool(std::string default_city, long timeout_sec)
        : default_city_(std::move(default_city)), timeout_(timeout_sec) {}

    std::string name() const override { return "weather"; }

    std::string description() const override {
        return "Ambil cuaca saat ini dan peluang hujan 6 jam ke depan untuk sebuah kota. "
               "Jika kota tidak diisi, dipakai kota default pengguna.";
    }

    nlohmann::json parameters() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"city", {{"type", "string"},
                          {"description", "Nama kota, mis. Pekanbaru. Opsional."}}}
            }},
            {"required", nlohmann::json::array()}
        };
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& args) const override {
        return "Cek cuaca: " + city_from(args);
    }

    std::string execute(const nlohmann::json& args) override {
        const std::string city = city_from(args);
        if (city.empty())
            return "Error: kota tidak diberikan dan default_location belum diatur di config.";
        try {
            return fetch(city);
        } catch (const std::exception& e) {
            return std::string("Error: gagal mengambil cuaca: ") + e.what();
        }
    }

private:
    std::string default_city_;
    long timeout_;
    HttpClient http_;

    std::string city_from(const nlohmann::json& args) const {
        if (args.is_object()) {
            auto it = args.find("city");
            if (it != args.end() && it->is_string() && !it->get<std::string>().empty())
                return it->get<std::string>().substr(0, 100);
        }
        return default_city_;
    }

    static std::string describe_code(int c) {
        switch (c) {
            case 0: return "cerah";
            case 1: return "cerah berawan";
            case 2: return "berawan sebagian";
            case 3: return "mendung";
            case 45: case 48: return "berkabut";
            case 51: case 53: case 55: return "gerimis";
            case 56: case 57: return "gerimis beku";
            case 61: return "hujan ringan";
            case 63: return "hujan sedang";
            case 65: return "hujan lebat";
            case 66: case 67: return "hujan beku";
            case 71: case 73: case 75: case 77: return "salju";
            case 80: case 81: return "hujan lokal";
            case 82: return "hujan lokal sangat lebat";
            case 85: case 86: return "hujan salju lokal";
            case 95: return "badai petir";
            case 96: case 99: return "badai petir dengan es";
            default: return "tidak diketahui";
        }
    }

    static std::string num(const nlohmann::json& j, const char* key, const char* fmt) {
        if (!j.is_object()) return "?";
        auto it = j.find(key);
        if (it == j.end() || !it->is_number()) return "?";
        char buf[32];
        std::snprintf(buf, sizeof buf, fmt, it->get<double>());
        return buf;
    }

    std::string fetch(const std::string& city) const {
        // 1) geocoding
        const std::string geo_url =
            "https://geocoding-api.open-meteo.com/v1/search?name=" +
            HttpClient::url_encode(city) + "&count=1&language=id&format=json";
        HttpResponse g = http_.get(geo_url, {}, timeout_, 64 * 1024);
        if (g.statusCode != 200) return "Error: geocoding HTTP " + std::to_string(g.statusCode);
        auto gj = nlohmann::json::parse(g.body, nullptr, false);
        if (gj.is_discarded() || !gj.contains("results") || !gj["results"].is_array() ||
            gj["results"].empty())
            return "Kota \"" + city + "\" tidak ditemukan.";
        const auto& r0 = gj["results"][0];
        if (!r0.contains("latitude") || !r0.contains("longitude") ||
            !r0["latitude"].is_number() || !r0["longitude"].is_number())
            return "Error: respons geocoding tidak valid.";

        char coord[64];
        std::snprintf(coord, sizeof coord, "latitude=%.4f&longitude=%.4f",
                      r0["latitude"].get<double>(), r0["longitude"].get<double>());
        std::string place = city;
        if (r0.contains("name") && r0["name"].is_string()) place = r0["name"].get<std::string>();
        if (r0.contains("country") && r0["country"].is_string())
            place += ", " + r0["country"].get<std::string>();

        // 2) prakiraan
        const std::string url =
            std::string("https://api.open-meteo.com/v1/forecast?") + coord +
            "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
            "wind_speed_10m,weather_code,precipitation"
            "&hourly=precipitation_probability&forecast_hours=6"
            "&timezone=auto&temperature_unit=celsius&wind_speed_unit=kmh";
        HttpResponse f = http_.get(url, {}, timeout_, 128 * 1024);
        if (f.statusCode != 200) return "Error: prakiraan HTTP " + std::to_string(f.statusCode);
        auto fj = nlohmann::json::parse(f.body, nullptr, false);
        if (fj.is_discarded() || !fj.contains("current") || !fj["current"].is_object())
            return "Error: respons cuaca tidak valid.";
        const auto& cur = fj["current"];

        int code = -1;
        if (cur.contains("weather_code") && cur["weather_code"].is_number_integer())
            code = cur["weather_code"].get<int>();

        std::string out = "Cuaca " + place + ": " + describe_code(code) + ", " +
            num(cur, "temperature_2m", "%.0f") + "°C (terasa " +
            num(cur, "apparent_temperature", "%.0f") + "°C), kelembapan " +
            num(cur, "relative_humidity_2m", "%.0f") + "%, angin " +
            num(cur, "wind_speed_10m", "%.0f") + " km/j, curah hujan " +
            num(cur, "precipitation", "%.1f") + " mm.";

        if (fj.contains("hourly") && fj["hourly"].is_object() &&
            fj["hourly"].contains("precipitation_probability") &&
            fj["hourly"]["precipitation_probability"].is_array()) {
            out += " Peluang hujan 6 jam ke depan:";
            for (const auto& p : fj["hourly"]["precipitation_probability"]) {
                char b[16];
                std::snprintf(b, sizeof b, " %d%%",
                              p.is_number() ? static_cast<int>(p.get<double>()) : -1);
                out += b;
            }
            out += ".";
        }
        return out;
    }
};