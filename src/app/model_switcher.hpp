#pragma once

#include "utils/config.hpp"

#include <cstddef>
#include <string>
#include <vector>

// Antarmuka ganti model saat berjalan. Diimplementasikan oleh App (main.cpp),
// supaya commands.cpp tidak perlu mengenal backend_factory.
class ModelSwitcher {
public:
    virtual ~ModelSwitcher() = default;

    // Semua pilihan provider+model (urutan tetap; nomor tampilan = indeks + 1).
    virtual const std::vector<ModelChoice>& choices() const = 0;

    // Indeks pilihan yang sedang aktif.
    virtual std::size_t active() const = 0;

    // Teks untuk header TUI: "provider · model".
    virtual std::string label() const = 0;

    // Pindah ke pilihan `index`. Mengembalikan "" jika sukses, atau pesan error
    // (dalam hal itu model lama tetap dipakai).
    virtual std::string switch_to(std::size_t index) = 0;
};