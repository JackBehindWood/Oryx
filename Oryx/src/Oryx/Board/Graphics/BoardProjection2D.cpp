#include "BoardProjection2D.h"

namespace oryx
{

Rect board_region_2d(const Vec2f& viewport)
{
    return { { k_board_gutter, k_board_status_band }, { viewport[0] - 2.0f * k_board_gutter, viewport[1] - k_board_status_band - k_board_menu_band - k_board_gutter } };
}

BoardProjection2D fit_board_2d(const BoardScene& scene, const Vec2f& viewport)
{
    return fit_board_2d(scene, viewport, board_region_2d(viewport));
}

BoardProjection2D fit_board_2d(const BoardScene& scene, const Vec2f& viewport, const Rect& region)
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
    float left = region.min[0];
    float bottom = viewport[1] - region.min[1] - region.size[1];
    float width = region.size[0];
    float height = region.size[1];
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

} // namespace oryx
