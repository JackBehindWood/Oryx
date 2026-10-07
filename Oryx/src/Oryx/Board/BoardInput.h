#pragma once

#include "Oryx/Core/Input.h"
#include "Oryx/Math/Vector2.h"

namespace oryx
{

// Shown on a finished windowed game; read_board_input maps exactly these inputs to `restart`.
constexpr const char* k_restart_hint = "press R to play again";

// One frame of what a windowed board may react to, captured once by BoardLayer so boards never read Input or the window themselves.
// Positions are logical window points with the origin top left, the units of NativeWindowHandle's width and height.
struct BoardInput
{
    Vec2f viewport;
    // Device pixels per logical point.
    float scale = 1.0f;
    Vec2f cursor;
    // Left click.
    bool select = false;
    // Left button held, and released this frame: with `select` they carry a click or a drag.
    bool select_down = false;
    bool select_released = false;
    // Right click, Backspace, or Escape while the left button is held: take back the last pick of a move being built.
    bool back = false;
    // Enter: play a move that is legal but could continue.
    bool confirm = false;
    // U: take back the last move.
    bool undo = false;
    // R; BoardLayer honours it only on a finished game. A board's own Play again button asks for the same thing.
    bool restart = false;
    // Escape with no button held.
    bool quit = false;
};

// The single mapping from devices to board intents.
[[nodiscard]] BoardInput read_board_input(const IInput& input, const Vec2f& viewport, float scale = 1.0f);

} // namespace oryx
