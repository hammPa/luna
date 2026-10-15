#pragma once

#include "tools/prompter.hpp"
#include "tools/tool.hpp"

#include <map>
#include <set>
#include <string>

enum class Policy { Allow, Ask, Deny };

struct PermissionPolicy {
    // Default ketat: yang tidak disebut = Ask.
    std::map<Risk, Policy> by_risk = {
        {Risk::Safe, Policy::Allow},
        {Risk::Write, Policy::Ask},
        {Risk::Destructive, Policy::Ask},
        {Risk::Execute, Policy::Ask},
        {Risk::Privileged, Policy::Ask},
    };
};

struct GateResult {
    bool allowed = false;
    std::string reason;  // dikirim balik ke model kalau ditolak
};

class PermissionGate {
public:
    PermissionGate(PermissionPolicy policy, Prompter& prompter);

    // Putuskan apakah pemanggilan ini boleh jalan.
    GateResult check(const Tool& tool, const nlohmann::json& args);
    
    // Lempar runtime_error kalau ada kunci atau nilai tidak dikenal.
private:
    PermissionPolicy policy_;
    Prompter& prompter_;
    std::set<std::string> always_;  // nama tool yang di-"always" untuk sesi ini (RAM saja)
};

PermissionPolicy make_policy(const std::map<std::string, std::string>& cfg);
