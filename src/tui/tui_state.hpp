#pragma once

#include "tui/tui_render.hpp"
#include "tui/tui_text.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// Seluruh state TUI di satu tempat.
struct TuiState {
    // Dibagi dengan thread kerja.
    std::mutex mu;                 // melindungi lines
    std::vector<Line> lines;
    std::atomic<bool> busy{false};
    std::atomic<bool> quit{false};
    std::atomic<int> frame{0};
    std::atomic<bool> cancelling{false};   // Esc sudah ditekan untuk giliran ini

    // Hanya disentuh thread UI (event dan render): tanpa mutex.
    std::string input;
    int cursor = 0;                // posisi kursor kotak input
    int scroll = 0;                // baris dari bawah; 0 = ikuti pesan terbaru
    int last_total = 0;            // jumlah baris isi pada render sebelumnya
    int menu_sel = 0;
    std::string menu_input;        // untuk mereset pilihan saat input berubah

    void add(const std::string& role, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu);
        lines.push_back({role, tui_text::sanitize(msg)});
    }
};