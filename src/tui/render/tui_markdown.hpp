#pragma once

#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

#include <string>

// Teks markdown menjadi daftar elemen, SATU ELEMEN = SATU BARIS LAYAR.
// width = lebar kolom yang tersedia untuk isi (di luar lencana).
// Selalu minimal satu elemen.
ftxui::Elements markdown_rows(const std::string& content, ftxui::Color body, int width);