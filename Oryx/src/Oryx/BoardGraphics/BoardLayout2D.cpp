#include "BoardLayout2D.h"

namespace oryx
{

namespace
{

constexpr Vec2f kOptionButtonSize = { 120.0f, 36.0f };
constexpr float kOptionButtonGap = 12.0f;

} // namespace

BoardLayout2D fit_board_2d(const BoardScene& scene, const Vec2f& viewport)
{
    BoardLayout2D layout;
    layout.viewport = viewport;
    layout.scale = 0.0f;

    float board_width = scene.max[0] - scene.min[0];
    float board_height = scene.max[1] - scene.min[1];
    float left = kBoardGutter;
    float bottom = kBoardMenuBand + kBoardGutter;
    float width = viewport[0] - 2.0f * kBoardGutter;
    float height = viewport[1] - kBoardStatusBand - bottom;
    if (board_width <= 0.0f || board_height <= 0.0f || width <= 0.0f || height <= 0.0f)
    {
        return layout;
    }

    layout.scale = std::min(width / board_width, height / board_height);
    layout.origin = {
        left + (width - board_width * layout.scale) * 0.5f - scene.min[0] * layout.scale,
        bottom + (height - board_height * layout.scale) * 0.5f - scene.min[1] * layout.scale,
    };
    return layout;
}

Vec2f board_to_world(const BoardLayout2D& layout, const Vec2f& board)
{
    return { layout.origin[0] + board[0] * layout.scale, layout.origin[1] + board[1] * layout.scale };
}

Vec2f cursor_to_world(const BoardLayout2D& layout, const Vec2f& cursor)
{
    return { cursor[0], layout.viewport[1] - cursor[1] };
}

Vec2f cursor_to_board(const BoardLayout2D& layout, const Vec2f& cursor)
{
    if (layout.scale <= 0.0f)
    {
        return { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };
    }
    Vec2f world = cursor_to_world(layout, cursor);
    return { (world[0] - layout.origin[0]) / layout.scale, (world[1] - layout.origin[1]) / layout.scale };
}

void option_buttons_2d(const BoardLayout2D& layout, size_t count, std::vector<OptionButton2D>& out)
{
    out.clear();
    float total = static_cast<float>(count) * kOptionButtonSize[0] + static_cast<float>(count > 0 ? count - 1 : 0) * kOptionButtonGap;
    float x = (layout.viewport[0] - total) * 0.5f + kOptionButtonSize[0] * 0.5f;
    float y = kBoardMenuBand * 0.5f;
    for (size_t index = 0; index < count; ++index)
    {
        out.push_back({ { x, y }, kOptionButtonSize });
        x += kOptionButtonSize[0] + kOptionButtonGap;
    }
}

size_t option_at(const std::vector<OptionButton2D>& buttons, const BoardLayout2D& layout, const Vec2f& cursor)
{
    Vec2f world = cursor_to_world(layout, cursor);
    for (size_t index = 0; index < buttons.size(); ++index)
    {
        const OptionButton2D& button = buttons[index];
        if (std::abs(world[0] - button.centre[0]) <= button.size[0] * 0.5f && std::abs(world[1] - button.centre[1]) <= button.size[1] * 0.5f)
        {
            return index;
        }
    }
    return buttons.size();
}

} // namespace oryx
