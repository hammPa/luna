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


// Bilah petunjuk bawah; rows_below > 0 = sedang menggulir.
ftxui::Element hint_bar(int rows_below);

// Header: nama, status backend, indikator konteks, siap/bekerja.
ftxui::Element header_bar(const std::string& status, int used, int max,
                          bool estimated, bool busy);

// Baris "luna berpikir..." dengan spinner.
ftxui::Element busy_row(int frame);

// Kotak input berbingkai. input_el = hasil input_box->Render().
ftxui::Element input_bar(ftxui::Element input_el, bool busy);

// Kotak izin di tengah layar (dipakai dengan dbox).
ftxui::Element permission_box(const std::string& question, bool can_always);