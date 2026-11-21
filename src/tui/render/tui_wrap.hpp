#pragma once

#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "utils/markdown.hpp"

#include <string>
#include <vector>

// Bungkus span markdown menjadi baris layar (satu elemen = satu baris).
// prefix_first dan prefix_rest = awalan tiap baris (mis. "• " lalu spasi
// selebar itu); lebarnya harus sama.
ftxui::Elements wrap_spans(const std::vector<md::Span>& spans, bool strong,
                           ftxui::Color base, int width,
                           const std::string& prefix_first,
                           const std::string& prefix_rest,
                           ftxui::Color prefix_color);