#include "tools/permission.hpp"

namespace {

const char* risk_label(Risk r) {
    switch (r) {
        case Risk::Safe:        return "aman";
        case Risk::Write:       return "menulis";
        case Risk::Destructive: return "MENGHAPUS/MENIMPA";
        case Risk::Execute:     return "MENJALANKAN PROGRAM";
        case Risk::Privileged:  return "HAK ISTIMEWA (sudo)";
    }
    return "?";
}

// Tiga tingkat ini tidak boleh lolos otomatis, apa pun isi config.
bool never_auto(Risk r) {
    return r == Risk::Destructive || r == Risk::Execute || r == Risk::Privileged;
}

}  // namespace

PermissionGate::PermissionGate(PermissionPolicy policy, Prompter& prompter)
    : policy_(std::move(policy)), prompter_(prompter) {}

GateResult PermissionGate::check(const Tool& tool, const nlohmann::json& args) {
    const Risk risk = tool.risk(args);

    Policy policy = Policy::Ask;  // tidak terdaftar = tanya
    auto it = policy_.by_risk.find(risk);
    if (it != policy_.by_risk.end()) policy = it->second;

    if (policy == Policy::Deny) {
        return {false, "Ditolak oleh kebijakan: tool '" + tool.name() +
                           "' tidak diizinkan (" + risk_label(risk) + ")."};
    }

    if (policy == Policy::Allow) {
        if (!never_auto(risk)) return {true, ""};
        policy = Policy::Ask;  // dipaksa tanya
    }

    // policy == Ask
    // "Always" hanya berlaku untuk Write. Kunci per tool.
    if (risk == Risk::Write && always_.count(tool.name()) != 0) {
        return {true, ""};
    }

    const bool can_always = (risk == Risk::Write);
    std::string question = std::string("\n[IZIN] ") + risk_label(risk) +
                           "\n  Tool : " + tool.name() +
                           "\n  Aksi : " + tool.describe(args);

    Decision d = prompter_.ask(question, can_always);
    switch (d) {
        case Decision::AllowAlways:
            if (can_always) always_.insert(tool.name());
            return {true, ""};
        case Decision::AllowOnce:
            return {true, ""};
        case Decision::Deny:
            break;
    }
    return {false, "User menolak menjalankan '" + tool.name() +
                       "'. Jangan coba lagi tanpa diminta; jelaskan atau tawarkan alternatif."};
}