#pragma once

#include "ftxui/dom/elements.hpp"

#include <string>

class ChatSession;
class TuiPrompter;
struct TuiState;

// Menyusun seluruh layar untuk satu frame (dipanggil dari Renderer).
ftxui::Element render_screen(TuiState& st, ChatSession& session, TuiPrompter& prompter,
                             const std::string& status, ftxui::Element input_el);