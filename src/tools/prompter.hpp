#pragma once

#include <string>

enum class Decision { AllowOnce, AllowAlways, Deny };

// Cara menanyakan izin ke user. Interface supaya gerbang bisa dites
// tanpa mengetik, dan nanti tidak terikat ke terminal.
class Prompter {
public:
    virtual ~Prompter() = default;

    // `can_always` = false berarti opsi "always" tidak ditawarkan.
    virtual Decision ask(const std::string& question, bool can_always) = 0;
};

// Bertanya lewat terminal (stdin/stdout).
class TerminalPrompter : public Prompter {
public:
    Decision ask(const std::string& question, bool can_always) override;
};