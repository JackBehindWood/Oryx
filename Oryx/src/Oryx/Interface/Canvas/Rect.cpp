#include "oxpch.h"
#include "Oryx/Interface/Canvas/Rect.h"

namespace oryx
{

bool contains(const Rect& rect, const Vec2f& point)
{
    return point[0] >= rect.min[0] && point[1] >= rect.min[1] && point[0] < rect.min[0] + rect.size[0] && point[1] < rect.min[1] + rect.size[1];
}

Rect intersect(const Rect& a, const Rect& b)
{
    const float left = std::max(a.min[0], b.min[0]);
    const float top = std::max(a.min[1], b.min[1]);
    const float right = std::min(a.min[0] + a.size[0], b.min[0] + b.size[0]);
    const float bottom = std::min(a.min[1] + a.size[1], b.min[1] + b.size[1]);
    return { Vec2f(left, top), Vec2f(std::max(right - left, 0.0f), std::max(bottom - top, 0.0f)) };
}

Rect unbounded_rect()
{
    const float limit = std::numeric_limits<float>::max() * 0.25f;
    return { Vec2f(-limit * 0.5f, -limit * 0.5f), Vec2f(limit, limit) };
}

bool overlaps(const Rect& a, const Rect& b)
{
    return !is_empty(intersect(a, b));
}

Rect inset(const Rect& rect, const Insets& insets)
{
    return { Vec2f(rect.min[0] + insets.left, rect.min[1] + insets.top), Vec2f(std::max(rect.size[0] - insets.left - insets.right, 0.0f), std::max(rect.size[1] - insets.top - insets.bottom, 0.0f)) };
}

Rect expand(const Rect& rect, const Insets& insets)
{
    return { Vec2f(rect.min[0] - insets.left, rect.min[1] - insets.top), Vec2f(rect.size[0] + insets.left + insets.right, rect.size[1] + insets.top + insets.bottom) };
}

Rect at_least(const Rect& rect, const Vec2f& minimum)
{
    const float width = std::max(rect.size[0], minimum[0]);
    const float height = std::max(rect.size[1], minimum[1]);
    return { Vec2f(rect.min[0] - (width - rect.size[0]) * 0.5f, rect.min[1] - (height - rect.size[1]) * 0.5f), Vec2f(width, height) };
}

CornerRadius clamp_radius(const CornerRadius& radius, const Vec2f& size)
{
    CornerRadius out = { std::max(radius.top_left, 0.0f), std::max(radius.top_right, 0.0f), std::max(radius.bottom_right, 0.0f), std::max(radius.bottom_left, 0.0f) };
    float scale = 1.0f;
    const float sums[4] = { out.top_left + out.top_right, out.bottom_left + out.bottom_right, out.top_left + out.bottom_left, out.top_right + out.bottom_right };
    const float limits[4] = { size[0], size[0], size[1], size[1] };
    for (uint32_t i = 0; i < 4; ++i)
    {
        if (sums[i] > limits[i] && sums[i] > 0.0f)
        {
            scale = std::min(scale, std::max(limits[i], 0.0f) / sums[i]);
        }
    }
    return { out.top_left * scale, out.top_right * scale, out.bottom_right * scale, out.bottom_left * scale };
}

bool is_square(const CornerRadius& radius)
{
    return !(radius.top_left > 0.0f) && !(radius.top_right > 0.0f) && !(radius.bottom_right > 0.0f) && !(radius.bottom_left > 0.0f);
}

float snap_to_pixel(float value, float scale)
{
    return scale > 0.0f ? std::round(value * scale) / scale : value;
}

Rect snap_to_pixels(const Rect& rect, float scale)
{
    const Vec2f max = rect_max(rect);
    const Vec2f min(snap_to_pixel(rect.min[0], scale), snap_to_pixel(rect.min[1], scale));
    return { min, Vec2f(snap_to_pixel(max[0], scale) - min[0], snap_to_pixel(max[1], scale) - min[1]) };
}

} // namespace oryx
