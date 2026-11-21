#pragma once

#include "tui/core/tui_state.hpp"

#include "ftxui/component/event.hpp"

#include <string>

// Simpan pesan yang dikirim ke riwayat input (duplikat berurutan dilewati).
void push_history(TuiState& st, const std::string& msg);

// Panah atas/bawah menelusuri riwayat input. true = event ditangani.
bool handle_history(const ftxui::Event& e, TuiState& st);