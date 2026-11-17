#pragma once
#include <memory>
#include "search_provider.hpp"
#include "../utils/config.hpp"

// nullptr = pencarian mati (config kosong / key tidak ada). Fail closed.
std::unique_ptr<SearchProvider> make_search_provider(const Config& cfg);