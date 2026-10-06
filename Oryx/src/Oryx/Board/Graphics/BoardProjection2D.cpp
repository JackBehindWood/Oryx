#include "BoardProjection2D.h"

namespace oryx
{

namespace
{

constexpr Vec2f k_option_button_size = { 120.0f, 36.0f };
constexpr float k_option_button_gap = 12.0f;

} // namespace

BoardProjection2D fit_board_2d(const BoardScene& scene, const Vec2f& viewport)
{
    BoardProjection2D layout;
    layout.viewport = viewport;
    layout.scale = 0.0f;

    if (scene.layout == nullptr || !std::isfinite(viewport[0]) || !std::isfinite(viewport[1]))
    {
        return layout;
    }
    const Vec2f& min = scene.layout->min();
    const Vec2f& max = scene.layout->max();
    float board_width = max[0] - min[0];
    float board_height = max[1] - min[1];
    float left = k_board_gutter;
    float bottom = k_board_menu_band + k_board_gutter;
    float width = viewport[0] - 2.0f * k_board_gutter;
    float height = viewport[1] - k_board_status_band - bottom;
    if (board_width <= 0.0f || board_height <= 0.0f || width <= 0.0f || height <= 0.0f)
    {
        return layout;
    }

    layout.scale = std::min(width / board_width, height / board_height);
    layout.origin = {
        left + (width - board_width * layout.scale) * 0.5f - min[0] * layout.scale,
        bottom + (height - board_height * layout.scale) * 0.5f - min[1] * layout.scale,
    };
    return layout;
}

Vec2f board_to_world(const BoardProjection2D& layout, const Vec2f& board)
{
    return { layout.origin[0] + board[0] * layout.scale, layout.origin[1] + board[1] * layout.scale };
}

Vec2f cursor_to_world(const BoardProjection2D& layout, const Vec2f& cursor)
{
    return { cursor[0], layout.viewport[1] - cursor[1] };
}

Vec2f cursor_to_board(const BoardProjection2D& layout, const Vec2f& cursor)
{
    if (layout.scale <= 0.0f)
    {
        return { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };
    }
    Vec2f world = cursor_to_world(layout, cursor);
    return { (world[0] - layout.origin[0]) / layout.scale, (world[1] - layout.origin[1]) / layout.scale };
}

OptionButton2D option_button_2d(const BoardProjection2D& layout, size_t count, size_t index)
{
    float step = k_option_button_size[0] + k_option_button_gap;
    float total = static_cast<float>(count) * k_option_button_size[0] + static_cast<float>(count > 0 ? count - 1 : 0) * k_option_button_gap;
    float x = (layout.viewport[0] - total) * 0.5f + k_option_button_size[0] * 0.5f + static_cast<float>(index) * step;
    return { { x, k_board_menu_band * 0.5f }, k_option_button_size };
}

void option_buttons_2d(const BoardProjection2D& layout, size_t count, std::vector<OptionButton2D>& out)
{
    out.clear();
    for (size_t index = 0; index < count; ++index)
    {
        out.push_back(option_button_2d(layout, count, index));
    }
}

size_t option_at(const std::vector<OptionButton2D>& buttons, const BoardProjection2D& layout, const Vec2f& cursor)
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
