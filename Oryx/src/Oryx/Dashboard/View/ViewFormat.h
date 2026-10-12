#pragma once

#include "Oryx/Interface/Canvas/FrameArena.h"

namespace oryx
{

// Strategy-agnostic number text: counts print as integers, everything else with four significant digits, NaN as a gap.
[[nodiscard]] inline std::string_view format_view_number(FrameArena& arena, double value)
{
    if (std::isnan(value))
    {
        return "-";
    }
    if (std::isinf(value))
    {
        return value > 0.0 ? "inf" : "-inf";
    }
    if (math::abs(value) < 1e15 && value == std::floor(value))
    {
        return arena.format("%.0f", value);
    }
    return arena.format("%.4g", value);
}

} // namespace oryx
