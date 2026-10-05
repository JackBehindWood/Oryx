#include "BoardInput.h"

namespace oryx
{

BoardInput read_board_input(const IInput& input, const Vec2f& viewport)
{
    BoardInput result;
    result.viewport = viewport;
    input.cursor_position(result.cursor);
    result.select = input.mouse_pressed(MouseCode::Left);
    result.back = input.mouse_pressed(MouseCode::Right) || input.key_pressed(KeyCode::Backspace);
    result.undo = input.key_pressed(KeyCode::U);
    result.restart = input.key_pressed(KeyCode::R) || result.select;
    result.quit = input.key_pressed(KeyCode::Escape);
    return result;
}

} // namespace oryx
