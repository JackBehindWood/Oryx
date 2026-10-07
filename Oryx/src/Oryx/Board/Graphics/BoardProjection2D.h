#pragma once

#include "Oryx/Board/BoardScene.h"
#include "Oryx/Interface/Canvas/Rect.h"

namespace oryx
{

// Where a board scene sits in a window, in world pixels (origin bottom left, y up, the pixel-unit Camera2D convention):
// world = origin + board * scale. Bands at the top (status) and bottom (option menu) stay free of the board.
struct BoardProjection2D
{
    Vec2f viewport;
    Vec2f origin;
    float scale = 1.0f;
};

constexpr float k_board_status_band = 56.0f;
constexpr float k_board_menu_band = 64.0f;
constexpr float k_board_gutter = 28.0f;

// The part of a viewport the board may use (logical points, origin top left): the status band on top, the menu band and gutters elsewhere.
[[nodiscard]] Rect board_region_2d(const Vec2f& viewport);

// Fits the scene's extent into the viewport, centred and uniformly scaled; a degenerate viewport or scene gives scale 0.
[[nodiscard]] BoardProjection2D fit_board_2d(const BoardScene& scene, const Vec2f& viewport);
// Same, inside `region` (logical points, origin top left) of the viewport.
[[nodiscard]] BoardProjection2D fit_board_2d(const BoardScene& scene, const Vec2f& viewport, const Rect& region);

[[nodiscard]] Vec2f board_to_world(const BoardProjection2D& layout, const Vec2f& board);
// Cursor (logical points, origin top left, y down) to board units.
[[nodiscard]] Vec2f cursor_to_board(const BoardProjection2D& layout, const Vec2f& cursor);
[[nodiscard]] Vec2f cursor_to_world(const BoardProjection2D& layout, const Vec2f& cursor);

} // namespace oryx
