#include "BoardInput.h"

namespace oryx
{

BoardInput read_board_input(const IInput& input, const Vec2f& viewport, float scale)
{
    BoardInput result;
    result.viewport = viewport;
    result.scale = scale;
    input.cursor_position(result.cursor);
    result.select = input.mouse_pressed(MouseCode::Left);
    result.select_down = input.mouse_down(MouseCode::Left);
    result.select_released = input.mouse_released(MouseCode::Left);
    bool escape = input.key_pressed(KeyCode::Escape);
    result.back = input.mouse_pressed(MouseCode::Right) || input.key_pressed(KeyCode::Backspace) || (escape && result.select_down);
    result.confirm = input.key_pressed(KeyCode::Enter);
    result.undo = input.key_pressed(KeyCode::U);
    result.restart = input.key_pressed(KeyCode::R);
    result.quit = escape && !result.select_down;
    return result;
}

} // namespace oryx
