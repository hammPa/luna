#include "tools/tool_registry.hpp"
#include "utils/log.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

ToolRegistry::ToolRegistry(PermissionGate& gate) : gate_(gate) {}

void ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) {
        throw std::invalid_argument("Tool null");
    }
    std::string name = tool->name();
    if (tools_.count(name) != 0) {
        throw std::runtime_error("Tool sudah terdaftar: " + name);
    }
    tools_.emplace(std::move(name), std::move(tool));
}

nlohmann::json ToolRegistry::schema() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [name, tool] : tools_) {
        if (disabled_.count(name) != 0) continue;
        arr.push_back({
            {"name", name},
            {"description", tool->description()},
            {"parameters", tool->parameters()},
        });
    }
    return arr;
}

std::string ToolRegistry::run(const std::string& name, const nlohmann::json& args) {
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        logger::warn("tool", "tool tidak ada: " + name);
        return "Error: tool '" + name + "' tidak ada.";
    }
    if (disabled_.count(name) != 0) {
        logger::warn("tool", "tool nonaktif: " + name);
        return "Error: tool '" + name + "' sedang dinonaktifkan.";
    }
    Tool& tool = *it->second;

    using clock = std::chrono::steady_clock;
    auto ms_since = [](clock::time_point t) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t).count();
    };

    try {
        const auto t_gate = clock::now();
        GateResult g = gate_.check(tool, args);
        const auto wait_ms = ms_since(t_gate);   // termasuk waktu menunggu persetujuan

        if (!g.allowed) {
            logger::warn("tool", name + " ditolak wait_ms=" + std::to_string(wait_ms));
            return g.reason;
        }

        const auto t_run = clock::now();
        std::string result = tool.execute(args);

        // Tool melaporkan kegagalan sebagai teks "Error...", bukan exception.
        const bool failed = result.rfind("Error", 0) == 0;
        const std::string line = name + (failed ? " gagal" : " selesai") +
                                 " wait_ms=" + std::to_string(wait_ms) +
                                 " ms=" + std::to_string(ms_since(t_run)) +
                                 " hasil_chars=" + std::to_string(result.size());
        if (failed) logger::warn("tool", line);
        else logger::info("tool", line);
        return result;
    } catch (const std::exception& e) {
        logger::error("tool", name + " exception: " + e.what());
        return std::string("Error saat menjalankan ") + name + ": " + e.what();
    }
}

std::string ToolRegistry::describe_all() const {
    std::string out;
    for (const auto& [name, tool] : tools_) {
        if (disabled_.count(name) != 0) continue;
        out += "- " + name + ": " + tool->description() + "\n";
    }
    return out;
}

std::vector<ToolStatus> ToolRegistry::list() const {
    std::vector<ToolStatus> out;
    for (const auto& [name, tool] : tools_) {
        out.push_back({name, disabled_.count(name) == 0});
    }
    return out;
}

bool ToolRegistry::set_enabled(const std::string& name, bool enabled) {
    if (tools_.count(name) == 0) return false;
    if (enabled) disabled_.erase(name);
    else disabled_.insert(name);
    return true;
}

void ToolRegistry::set_profiles(std::map<std::string, std::vector<std::string>> profiles) {
    profiles_ = std::move(profiles);
}

bool ToolRegistry::use_profile(const std::string& name, std::vector<std::string>& unknown) {
    auto it = profiles_.find(name);
    if (it == profiles_.end()) return false;

    unknown.clear();
    const auto& names = it->second;
    disabled_.clear();
    if (std::find(names.begin(), names.end(), "*") == names.end()) {
        for (const auto& [tool_name, tool] : tools_) disabled_.insert(tool_name);
        for (const auto& n : names) {
            if (tools_.count(n) == 0) unknown.push_back(n);
            else disabled_.erase(n);
        }
    }
    active_profile_ = name;
    return true;
}