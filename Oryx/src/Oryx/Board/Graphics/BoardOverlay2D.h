#pragma once

#include "Oryx/Board/BoardInput.h"
#include "Oryx/Board/BoardScene.h"
#include "Oryx/Board/Graphics/BoardProjection2D.h"
#include "Oryx/Interface/UI/Ui.h"

namespace oryx
{

struct OverlayButton
{
    Id id;
    Rect rect;
};

// The words of one frame, as views the caller keeps alive for the call. The variants name styles of the board's UiTheme (empty is the plain one).
struct BoardOverlayText
{
    // The line at the top: whose turn it is. Empty draws none.
    std::string_view status;
    std::string_view status_variant;
    // Non-empty once the game is over: the result banner with this headline, a hint under it and a Play again button.
    std::string_view headline;
    std::string_view headline_variant;
    std::string_view hint;
};

// What one frame of the overlay found out; the vectors keep their capacity so a warm frame allocates nothing.
struct BoardOverlayResult
{
    Vec2f viewport;
    // The area the board may use, from this frame's solve (logical points, origin top left); empty when the frame did not run.
    Rect board;
    std::vector<OverlayButton> buttons;
    // The option whose button was clicked this frame, valid when `chosen`.
    size_t option = 0;
    bool chosen = false;
    // The Play again button was clicked this frame.
    bool restart = false;
    // True while the pointer is over a button or holding one, so the board leaves the press to the UI.
    bool pointer_over_ui = false;
};

// The board's input as one frame of UI input. `pointer_captured` hides the pointer from the widgets (a drag on the board owns it).
[[nodiscard]] ImInput board_input_to_im(const BoardInput& input, double delta_time, bool pointer_captured);

// Runs one whole UiContext frame for the board overlay: the board box fills the surface, the status line floats at the top, the option buttons float at the bottom and, once the game is over, the result banner floats in the middle.
// The context's theme must carry the font. Throws what the context throws, after dropping the open frame.
void run_board_overlay(UiContext& ui, const BoardInput& input, double delta_time, bool pointer_captured, const BoardScene& scene, const BoardOverlayText& text, BoardOverlayResult& out);

} // namespace oryx
