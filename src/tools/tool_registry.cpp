#include "tools/tool_registry.hpp"

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
        return "Error: tool '" + name + "' tidak ada.";
    }
    Tool& tool = *it->second;

    try {
        GateResult g = gate_.check(tool, args);
        if (!g.allowed) {
            return g.reason;
        }
        return tool.execute(args);
    } catch (const std::exception& e) {
        return std::string("Error saat menjalankan ") + name + ": " + e.what();
    }
}

std::string ToolRegistry::describe_all() const {
    std::string out;
    for (const auto& [name, tool] : tools_) {
        out += "- " + name + ": " + tool->description() + "\n";
    }
    return out;
}