#pragma once

#include <stdexcept>

// Dilempar saat giliran dibatalkan pengguna. Bukan kegagalan.
struct CancelledError : std::runtime_error {
    CancelledError() : std::runtime_error("dibatalkan") {}
};