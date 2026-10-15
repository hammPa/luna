#pragma once

#include "tools/path_guard.hpp"   // arg_string
#include "tools/process_utils.hpp"
#include "tools/tool.hpp"

#include <map>

class OpenAppTool : public Tool {
public:
    explicit OpenAppTool(std::map<std::string, std::string> apps) : apps_(std::move(apps)) {
        for (const auto& [k, v] : apps_) {
            if (v.empty() || v.find(' ') != std::string::npos) {
                throw std::runtime_error("apps: nilai untuk '" + k +
                                         "' harus nama program tanpa spasi/argumen.");
            }
        }
    }

    std::string name() const override { return "open_app"; }

    std::string description() const override {
        std::string d = "Membuka aplikasi dari daftar yang diizinkan. Nama yang tersedia: ";
        bool first = true;
        for (const auto& [k, v] : apps_) {
            d += (first ? "" : ", ") + k;
            first = false;
        }
        return d + ".";
    }

    nlohmann::json parameters() const override {
        nlohmann::json names = nlohmann::json::array();
        for (const auto& [k, v] : apps_) names.push_back(k);
        return {{"type", "object"},
                {"properties", {{"name", {{"type", "string"}, {"enum", names},
                                          {"description", "Nama aplikasi dari daftar"}}}}},
                {"required", {"name"}}};
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Execute; }

    std::string describe(const nlohmann::json& args) const override {
        try {
            std::string n = arg_string(args, "name", "?");
            auto it = apps_.find(n);
            return "Menjalankan aplikasi '" + n + "'" +
                   (it != apps_.end() ? " (program: " + it->second + ")" : " (tidak ada di daftar)");
        } catch (const std::exception&) {
            return "Menjalankan aplikasi (argumen tidak valid)";
        }
    }

    std::string execute(const nlohmann::json& args) override {
        const std::string n = arg_string(args, "name", "");
        auto it = apps_.find(n);
        if (it == apps_.end()) {
            throw std::runtime_error("Aplikasi '" + n + "' tidak ada di daftar yang diizinkan.");
        }
        if (!program_exists(it->second)) {
            throw std::runtime_error("Program '" + it->second + "' tidak terpasang di sistem.");
        }
        spawn_detached({it->second});
        return "Berhasil menjalankan " + n + ".";
    }

private:
    std::map<std::string, std::string> apps_;
};