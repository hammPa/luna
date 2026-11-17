#pragma once

#include "tools/tool.hpp"

#include <ctime>

class GetTimeTool : public Tool {
public:
    std::string name() const override { return "get_time"; }

    std::string description() const override {
        return "Mengembalikan tanggal dan jam saat ini (waktu lokal pengguna).";
    }

    nlohmann::json parameters() const override {
        // Tidak ada argumen, tapi schema object kosong tetap harus valid.
        return {{"type", "object"},
                {"properties", nlohmann::json::object()}};
    }

    Risk risk(const nlohmann::json& /*args*/) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& /*args*/) const override {
        return "Membaca jam sistem";
    }

    std::string execute(const nlohmann::json& /*args*/) override {
        std::time_t now = std::time(nullptr);
        std::tm tm_buf{};
        localtime_r(&now, &tm_buf);  // versi thread-safe (POSIX/Linux)

        char buf[64];
        if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf) == 0) {
            throw std::runtime_error("Gagal memformat waktu");
        }
        return buf;
    }
};