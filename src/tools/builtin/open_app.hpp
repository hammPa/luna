#pragma once

#include "tools/path_guard.hpp"   // arg_string
#include "tools/process_utils.hpp"
#include "tools/tool.hpp"
#include "utils/config.hpp"       // AppEntry

#include <map>

class OpenAppTool : public Tool {
public:
    explicit OpenAppTool(std::map<std::string, AppEntry> apps) : apps_(std::move(apps)) {
        for (const auto& [k, e] : apps_) {
            if (e.run.empty() || e.run.find(' ') != std::string::npos) {
                throw std::runtime_error("apps: 'run' untuk '" + k +
                                         "' harus nama program tanpa spasi/argumen.");
            }
        }
    }

    std::string name() const override { return "open_app"; }

    std::string description() const override {
        std::string d = "Membuka aplikasi dari daftar yang diizinkan. Nama yang tersedia: ";
        bool first = true;
        for (const auto& [k, e] : apps_) {
            d += (first ? "" : ", ") + k;
            first = false;
        }
        return d + ".";
    }

    nlohmann::json parameters() const override {
        nlohmann::json names = nlohmann::json::array();
        for (const auto& [k, e] : apps_) names.push_back(k);
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
                   (it != apps_.end() ? " (program: " + it->second.run + ")"
                                      : " (tidak ada di daftar)");
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
        if (!program_exists(it->second.run)) {
            throw std::runtime_error("Program '" + it->second.run + "' tidak terpasang di sistem.");
        }
        spawn_detached({it->second.run});
        return "Berhasil menjalankan " + n + ".";
    }

private:
    std::map<std::string, AppEntry> apps_;
};