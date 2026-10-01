#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

enum class SeedRole : uint8_t
{
    Game,
    Strategy,
    Experiment
};

[[nodiscard]] uint64_t hash_string(std::string_view text);

// Depends only on its arguments, never on call order; `seat` distinguishes strategy streams.
[[nodiscard]] uint64_t derive_seed(uint64_t master, std::string_view key, SeedRole role, uint32_t seat = 0, uint32_t repeat = 0);

} // namespace oryx
