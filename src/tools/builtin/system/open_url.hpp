#pragma once

#include "tools/path_guard.hpp"   // arg_string
#include "utils/process_utils.hpp"
#include "tools/tool.hpp"

class OpenUrlTool : public Tool {
public:
    std::string name() const override { return "open_url"; }

    std::string description() const override {
        return "Membuka URL (http/https) di browser default. Untuk mencari di YouTube atau "
               "Google, buat URL pencarian, mis. https://www.youtube.com/results?search_query=lofi "
               "(spasi ditulis +). Hanya membuka halaman, tidak bisa mengeklik atau membaca isinya.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties", {{"url", {{"type", "string"}, {"description", "URL lengkap, diawali https://"}}}}},
                {"required", {"url"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Execute; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            return "Membuka di browser: " + arg_string(args, "url", "?");
        } catch (const std::exception&) {
            return "Membuka URL (argumen tidak valid)";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        const std::string url = arg_string(args, "url", "");
        validate(url);
        if (!program_exists("xdg-open")) {
            throw std::runtime_error("xdg-open tidak ditemukan di sistem.");
        }
        spawn_detached({"xdg-open", url});
        return "Berhasil membuka " + url + " di browser.";
    }

private:
    static void validate(const std::string& url) {
        const bool https = url.rfind("https://", 0) == 0;
        const bool http = url.rfind("http://", 0) == 0;
        if (!https && !http) {
            throw std::runtime_error("Hanya URL http:// atau https:// yang diizinkan.");
        }
        if (url.size() > 2048) throw std::runtime_error("URL terlalu panjang.");
        for (unsigned char c : url) {
            if (c < 0x20 || c == 0x7f || c == ' ') {
                throw std::runtime_error("URL mengandung spasi atau karakter kontrol. "
                                         "Ganti spasi dengan + atau %20.");
            }
        }
    }
};