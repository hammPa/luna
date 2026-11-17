#pragma once

#include "ftxui/dom/elements.hpp"
#include "tui/slash_menu.hpp"
#include <vector>
#include <string>

struct Line {
    std::string role;   // user | luna | info | error
    std::string text;
};

// Banner LUNA sebagai daftar baris (versi singkat jika lebar < 40 kolom).
ftxui::Elements banner_rows(int width);

// Satu pesan jadi beberapa baris (satu per paragraf) + satu baris kosong pemisah.
ftxui::Elements line_rows(const Line& l);

// Teks banyak baris dengan pembungkusan kata.
ftxui::Element paragraphs(const std::string& content);

ftxui::Element slash_menu_view(const std::vector<SlashCmd>& items, int selected);
