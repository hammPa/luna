#pragma once

#include "tui/core/tui_state.hpp"

// /copy dan /copy code: salin balasan Luna terakhir (atau blok kode terakhir)
// ke clipboard, hasilnya dilaporkan lewat baris "info".
void do_copy(TuiState& st, bool code_only);