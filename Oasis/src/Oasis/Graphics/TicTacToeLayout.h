#pragma once

#include "Oryx/Math/Vector2.h"

namespace oasis
{

// Where the 3x3 grid sits in a window, in the same units as the window's size and cursor (y down from the top for the cursor, y up for the world).
struct TicTacToeLayout
{
    static constexpr float kBoardFraction = 0.7f;
    static constexpr float kStatusHeight = 24.0f;

    oryx::Vec2f viewport;
    oryx::Vec2f centre;
    float cell = 0.0f;

    [[nodiscard]] static TicTacToeLayout fit(const oryx::Vec2f& viewport)
    {
        TicTacToeLayout layout;
        layout.viewport = viewport;
        layout.centre = { viewport[0] * 0.5f, viewport[1] * 0.5f - kStatusHeight };
        layout.cell = kBoardFraction * (viewport[0] < viewport[1] ? viewport[0] : viewport[1]) / 3.0f;
        return layout;
    }

    [[nodiscard]] oryx::Vec2f world_from_cursor(const oryx::Vec2f& cursor) const
    {
        return { cursor[0], viewport[1] - cursor[1] };
    }

    [[nodiscard]] oryx::Vec2f cell_centre(size_t row, size_t col) const
    {
        return { centre[0] + (static_cast<float>(col) - 1.0f) * cell, centre[1] + (1.0f - static_cast<float>(row)) * cell };
    }

    [[nodiscard]] bool cell_at(const oryx::Vec2f& world, size_t& out_row, size_t& out_col) const
    {
        float x = (world[0] - centre[0]) / cell + 1.5f;
        float y = (centre[1] - world[1]) / cell + 1.5f;
        if (x < 0.0f || y < 0.0f || x >= 3.0f || y >= 3.0f)
        {
            return false;
        }
        out_col = static_cast<size_t>(x);
        out_row = static_cast<size_t>(y);
        return true;
    }
};

} // namespace oasis
