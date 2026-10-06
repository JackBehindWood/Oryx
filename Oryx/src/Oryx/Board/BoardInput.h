#pragma once

#include "Oryx/Core/Input.h"
#include "Oryx/Math/Vector2.h"

namespace oryx
{

// Shown on a finished windowed game; read_board_input maps exactly these inputs to `restart`.
constexpr const char* k_restart_hint = "click or press R to play again";

// One frame of what a windowed board may react to, captured once by BoardLayer so boards never read Input or the window themselves.
// Positions are logical window points with the origin top left, the units of NativeWindowHandle's width and height.
struct BoardInput
{
    Vec2f viewport;
    Vec2f cursor;
    // Left click.
    bool select = false;
    // Right click or Backspace: take back the last pick of a move being built.
    bool back = false;
    // U: take back the last move.
    bool undo = false;
    // R or left click; BoardLayer honours it only on a finished game.
    bool restart = false;
    // Escape.
    bool quit = false;
};

// The single mapping from devices to board intents.
[[nodiscard]] BoardInput read_board_input(const IInput& input, const Vec2f& viewport);

} // namespace oryx
