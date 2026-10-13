#pragma once

#include "tools/tool.hpp"

#include <iostream>

// HANYA UNTUK TES. Tidak melakukan apa-apa selain mencetak.
// Argumen "level" memilih tingkat risiko, supaya semua jalur gerbang bisa diuji.
class FakeDangerTool : public Tool {
public:
    std::string name() const override { return "fake_danger"; }

    std::string description() const override {
        return "Tool uji coba. Mencetak pesan saja.";
    }

    nlohmann::json parameters() const override {
        return {{"type", "object"},
                {"properties", {{"level", {{"type", "string"},
                                           {"enum", {"safe", "write", "destructive",
                                                     "execute", "privileged"}}}}}},
                {"required", {"level"}}};
    }

    Risk risk(const nlohmann::json& args) const override {
        const std::string level = args.value("level", "destructive");
        if (level == "safe") return Risk::Safe;
        if (level == "write") return Risk::Write;
        if (level == "execute") return Risk::Execute;
        if (level == "privileged") return Risk::Privileged;
        return Risk::Destructive;  // tidak dikenal = anggap paling berbahaya yang umum
    }

    std::string describe(const nlohmann::json& args) const override {
        return "(palsu) level " + args.value("level", "destructive");
    }

    std::string execute(const nlohmann::json& args) override {
        return "ANDAI INI NYATA: aksi level " + args.value("level", "destructive") + " dijalankan";
    }
};