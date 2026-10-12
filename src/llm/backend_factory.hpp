#pragma once

#include "llm/llm_backend.hpp"
#include "utils/config.hpp"

#include <memory>

// Lempar std::runtime_error untuk backend tak dikenal atau API key hilang.
std::unique_ptr<LLMBackend> create_backend(const Config& cfg);