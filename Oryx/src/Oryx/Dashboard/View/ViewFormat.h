#pragma once

#include "Oryx/Game/PlayerId.h"
#include "Oryx/Interface/Canvas/FrameArena.h"
#include "Oryx/Interface/GUI/GuiTheme.h"

namespace oryx
{

// A whole number with a comma between thousands: 549945 reads "549,945".
[[nodiscard]] inline std::string_view format_grouped(FrameArena& arena, double value)
{
    char digits[32];
    const int32_t length = std::snprintf(digits, sizeof(digits), "%.0f", math::abs(value));
    const size_t count = static_cast<size_t>(length);
    const size_t commas = (count - 1) / 3;
    const bool negative = value < 0.0;
    std::span<char> out = arena.allocate_array<char>(count + commas + (negative ? 1 : 0));
    size_t at = 0;
    if (negative)
    {
        out[at++] = '-';
    }
    for (size_t i = 0; i < count; ++i)
    {
        if (i > 0 && (count - i) % 3 == 0)
        {
            out[at++] = ',';
        }
        out[at++] = digits[i];
    }
    return std::string_view(out.data(), out.size());
}

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
        return format_grouped(arena, value);
    }
    return arena.format("%.4g", value);
}

// Players count from 1 in text; a chance or unset id has no number.
[[nodiscard]] inline std::string_view player_name(FrameArena& arena, PlayerId player)
{
    return player < 0 ? std::string_view("Chance") : arena.format("Player %d", static_cast<int32_t>(player) + 1);
}

// One fixed colour per seat, skipping the two hues the value bars use (blue and orange) so a player is never mistaken for a sign.
[[nodiscard]] inline Colour player_colour(const GuiTheme& theme, PlayerId player)
{
    constexpr uint32_t k_first = 2;
    const uint32_t seat = player < 0 ? 0u : static_cast<uint32_t>(player);
    return theme.palette[k_first + seat % (k_palette_size - k_first)];
}

// Blue for a value good for the player to move, orange for one that is bad (the hues of the value bars), quiet for zero or a gap.
[[nodiscard]] inline Colour value_colour(const GuiTheme& theme, double value)
{
    if (std::isnan(value) || value == 0.0)
    {
        return theme.caption_colour;
    }
    return value > 0.0 ? theme.palette[0] : theme.palette[1];
}

} // namespace oryx
