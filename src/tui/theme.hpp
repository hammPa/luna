#pragma once

#include "ftxui/screen/color.hpp"

// Semua warna TUI di satu tempat. Jangan beri nama "text" (menutupi ftxui::text()).
namespace theme {

inline const ftxui::Color accent   = ftxui::Color::RGB(198, 120, 221);  // ungu
inline const ftxui::Color muted    = ftxui::Color::RGB(130, 137, 151);  // abu
inline const ftxui::Color fg       = ftxui::Color::RGB(220, 223, 228);  // teks terang
inline const ftxui::Color dark     = ftxui::Color::RGB(30, 34, 40);     // teks di atas lencana
inline const ftxui::Color input_bg = ftxui::Color::RGB(40, 44, 52);
inline const ftxui::Color user_c   = ftxui::Color::RGB(86, 182, 194);   // cyan
inline const ftxui::Color luna_c   = ftxui::Color::RGB(152, 195, 121);  // hijau
inline const ftxui::Color error_c  = ftxui::Color::RGB(224, 108, 117);  // merah
inline const ftxui::Color warn_c   = ftxui::Color::RGB(229, 192, 123);  // kuning

// gradasi ungu -> biru, satu warna per baris logo
inline const ftxui::Color banner[6] = {
    ftxui::Color::RGB(198, 120, 221), ftxui::Color::RGB(180, 130, 230),
    ftxui::Color::RGB(160, 140, 235), ftxui::Color::RGB(140, 155, 240),
    ftxui::Color::RGB(115, 170, 242), ftxui::Color::RGB(97, 175, 239),
};

}  // namespace theme