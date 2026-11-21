#pragma once

#include <string>
#include <vector>

// Pecah string UTF-8 jadi potongan per codepoint (string asli + lebar kolomnya).
struct Glyph { std::string s; int w; };

std::vector<Glyph> glyphs(const std::string& in);

// Lebar tampil total string (kasar: CJK dan emoji = 2, sisanya 1).
int text_width(const std::string& s);