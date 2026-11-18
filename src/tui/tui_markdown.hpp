#pragma once

#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

#include <string>

// Teks markdown menjadi daftar elemen (satu per blok). Selalu minimal satu elemen.
ftxui::Elements markdown_rows(const std::string& content, ftxui::Color body);