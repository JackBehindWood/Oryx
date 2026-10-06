#pragma once

#include "Oryx/Board/BoardScene.h"

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

struct OptionButton2D
{
    Vec2f centre;
    Vec2f size;
};

constexpr float k_board_status_band = 56.0f;
constexpr float k_board_menu_band = 64.0f;
constexpr float k_board_gutter = 28.0f;

// Fits the scene's extent into the viewport, centred and uniformly scaled; a degenerate viewport or scene gives scale 0.
[[nodiscard]] BoardProjection2D fit_board_2d(const BoardScene& scene, const Vec2f& viewport);

[[nodiscard]] Vec2f board_to_world(const BoardProjection2D& layout, const Vec2f& board);
// Cursor (logical points, origin top left, y down) to board units.
[[nodiscard]] Vec2f cursor_to_board(const BoardProjection2D& layout, const Vec2f& cursor);
[[nodiscard]] Vec2f cursor_to_world(const BoardProjection2D& layout, const Vec2f& cursor);

// The `index`th of `count` buttons, centred in the bottom band.
[[nodiscard]] OptionButton2D option_button_2d(const BoardProjection2D& layout, size_t count, size_t index);
// All `count` buttons.
void option_buttons_2d(const BoardProjection2D& layout, size_t count, std::vector<OptionButton2D>& out);
// The index of the button under the cursor, or `buttons.size()`.
[[nodiscard]] size_t option_at(const std::vector<OptionButton2D>& buttons, const BoardProjection2D& layout, const Vec2f& cursor);

} // namespace oryx
