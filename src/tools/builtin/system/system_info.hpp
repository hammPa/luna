#pragma once

#include "tools/tool.hpp"

#include <sys/statvfs.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

// Membaca /proc dan /sys langsung (tanpa program luar). Tidak melaporkan
// hostname atau identitas, karena hasil tool dikirim ke provider LLM.
class SystemInfoTool : public Tool {
public:
    std::string name() const override { return "system_info"; }

    std::string description() const override {
        return "Info komputer: memori, beban CPU, disk (/), baterai, uptime.";
    }

    nlohmann::json parameters() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"section", {{"type", "string"},
                             {"enum", {"all", "memory", "cpu", "disk", "battery", "uptime"}},
                             {"description", "Bagian yang diminta. Bawaan: all."}}}
            }},
            {"required", nlohmann::json::array()}
        };
    }

    Risk risk(const nlohmann::json&) const override { return Risk::Safe; }

    std::string describe(const nlohmann::json& args) const override {
        return "Membaca info sistem: " + section_from(args);
    }

    std::string execute(const nlohmann::json& args) override {
        const std::string sec = section_from(args);
        static const std::vector<std::string> ok = {"all", "memory", "cpu", "disk",
                                                    "battery", "uptime"};
        if (std::find(ok.begin(), ok.end(), sec) == ok.end()) {
            return "Error: section tidak dikenal. Pilihan: all, memory, cpu, disk, battery, uptime.";
        }
        const bool all = (sec == "all");
        std::string out;
        auto add = [&](const std::string& line) {
            if (!out.empty()) out += "\n";
            out += line;
        };
        if (all || sec == "memory")  add(memory_line());
        if (all || sec == "cpu")     add(cpu_line());
        if (all || sec == "disk")    add(disk_line());
        if (all || sec == "battery") add(battery_line());
        if (all || sec == "uptime")  add(uptime_line());
        return out;
    }

private:
    static std::string section_from(const nlohmann::json& args) {
        if (args.is_object()) {
            auto it = args.find("section");
            if (it != args.end() && it->is_string() && !it->get<std::string>().empty())
                return it->get<std::string>();
        }
        return "all";
    }

    static std::string first_line(const std::string& path) {
        std::ifstream f(path);
        std::string s;
        std::getline(f, s);
        return s;
    }

    static std::string gb(double bytes) {
        char b[32];
        std::snprintf(b, sizeof b, "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
        return b;
    }

    static std::string memory_line() {
        std::ifstream f("/proc/meminfo");
        double total = 0, avail = 0, stotal = 0, sfree = 0;
        std::string line;
        while (std::getline(f, line)) {
            char key[64];
            unsigned long v = 0;
            if (std::sscanf(line.c_str(), "%63[^:]: %lu", key, &v) != 2) continue;
            const std::string k = key;
            const double bytes = static_cast<double>(v) * 1024.0;   // nilai dalam kB
            if (k == "MemTotal") total = bytes;
            else if (k == "MemAvailable") avail = bytes;
            else if (k == "SwapTotal") stotal = bytes;
            else if (k == "SwapFree") sfree = bytes;
        }
        if (total <= 0) return "Memori: tidak terbaca";
        const double used = total - avail;
        char pct[16];
        std::snprintf(pct, sizeof pct, "%.0f%%", used * 100.0 / total);
        std::string s = "Memori: terpakai " + gb(used) + " dari " + gb(total) + " (" + pct +
                        "), tersedia " + gb(avail);
        if (stotal > 0) s += "; swap terpakai " + gb(stotal - sfree) + " dari " + gb(stotal);
        return s;
    }

    static std::string cpu_line() {
        std::ifstream f("/proc/loadavg");
        double a = 0, b = 0, c = 0;
        if (!(f >> a >> b >> c)) return "CPU: beban tidak terbaca";
        char buf[96];
        std::snprintf(buf, sizeof buf, "CPU: %u core, beban rata-rata 1/5/15 menit: %.2f %.2f %.2f",
                      std::thread::hardware_concurrency(), a, b, c);
        return buf;
    }

    static std::string disk_line() {
        struct statvfs st {};
        if (statvfs("/", &st) != 0) return "Disk (/): tidak terbaca";
        const double total = static_cast<double>(st.f_blocks) * static_cast<double>(st.f_frsize);
        const double avail = static_cast<double>(st.f_bavail) * static_cast<double>(st.f_frsize);
        if (total <= 0) return "Disk (/): tidak terbaca";
        char pct[16];
        std::snprintf(pct, sizeof pct, "%.0f%%", (total - avail) * 100.0 / total);
        return "Disk (/): terpakai " + gb(total - avail) + " dari " + gb(total) + " (" + pct +
               "), tersisa " + gb(avail);
    }

    static std::string battery_line() {
        namespace fs = std::filesystem;
        const fs::path dir = "/sys/class/power_supply";
        std::error_code ec;
        std::string out;
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (first_line((e.path() / "type").string()) != "Battery") continue;
            const std::string cap = first_line((e.path() / "capacity").string());
            const std::string status = first_line((e.path() / "status").string());
            if (!out.empty()) out += "; ";
            out += (cap.empty() ? "?" : cap) + "%" + (status.empty() ? "" : " (" + status + ")");
        }
        return out.empty() ? "Baterai: tidak ada" : "Baterai: " + out;
    }

    static std::string uptime_line() {
        std::ifstream f("/proc/uptime");
        double secs = 0;
        if (!(f >> secs)) return "Uptime: tidak terbaca";
        const long total = static_cast<long>(secs);
        const long d = total / 86400, h = (total % 86400) / 3600, m = (total % 3600) / 60;
        std::string s = "Uptime:";
        if (d > 0) s += " " + std::to_string(d) + " hari";
        if (h > 0 || d > 0) s += " " + std::to_string(h) + " jam";
        s += " " + std::to_string(m) + " menit";
        return s;
    }
};