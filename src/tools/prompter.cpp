#include "tools/prompter.hpp"

#include <iostream>

Decision TerminalPrompter::ask(const std::string& question, bool can_always) {
    std::cout << question << '\n';

    while (true) {
        std::cout << (can_always ? "Izinkan? [y]a sekali / [a]lways sesi ini / [n]o : "
                                 : "Izinkan? [y]a sekali / [n]o : ")
                  << std::flush;

        std::string answer;
        if (!std::getline(std::cin, answer)) {
            return Decision::Deny;  // EOF = tolak, jangan pernah default ke izin
        }
        if (answer == "y" || answer == "Y") return Decision::AllowOnce;
        if (can_always && (answer == "a" || answer == "A")) return Decision::AllowAlways;
        if (answer == "n" || answer == "N") return Decision::Deny;

        std::cout << "Jawab y" << (can_always ? ", a," : "") << " atau n.\n";
    }
}