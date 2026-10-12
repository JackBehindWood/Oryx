#include "oxpch.h"
#include "Oryx/Interface/Canvas/TabShape.h"

namespace oryx
{

namespace
{

constexpr uint32_t k_shoulder_steps = 4;

// Appends the quadratic curve from `from` through the corner `corner` to `to`, ends included.
uint32_t append_curve(Vec2f* points, uint32_t count, const Vec2f& from, const Vec2f& corner, const Vec2f& to)
{
    for (uint32_t step = 0; step <= k_shoulder_steps; ++step)
    {
        const float t = static_cast<float>(step) / static_cast<float>(k_shoulder_steps);
        const float a = (1.0f - t) * (1.0f - t);
        const float b = 2.0f * (1.0f - t) * t;
        const float c = t * t;
        points[count++] = Vec2f(a * from[0] + b * corner[0] + c * to[0], a * from[1] + b * corner[1] + c * to[1]);
    }
    return count;
}

} // namespace

uint32_t tab_outline(const Rect& rect, float slope, float shoulder, bool flip, Vec2f (&points)[k_max_polygon_points])
{
    const float x0 = rect.min[0];
    const float x1 = rect.min[0] + rect.size[0];
    const float top = rect.min[1];
    const float base = rect.min[1] + rect.size[1];
    uint32_t count = 0;
    if (!(slope > 0.0f) || !(rect.size[0] > 0.0f) || !(rect.size[1] > 0.0f))
    {
        points[count++] = Vec2f(x0, base);
        points[count++] = Vec2f(x0, top);
        points[count++] = Vec2f(x1, top);
        points[count++] = Vec2f(x1, base);
    }
    else
    {
        const float lean = math::min(slope, rect.size[0] * 0.5f);
        const float height = rect.size[1];
        const float length = std::sqrt(lean * lean + height * height);
        const float reach = math::min(math::max(shoulder, 0.0f), math::min(length * 0.5f, (rect.size[0] - 2.0f * lean) * 0.5f));
        const Vec2f up(lean / length, -height / length);
        const Vec2f left_corner(x0 + lean, top);
        const Vec2f right_corner(x1 - lean, top);
        points[count++] = Vec2f(x0, base);
        count = append_curve(points, count, Vec2f(left_corner[0] - up[0] * reach, left_corner[1] - up[1] * reach), left_corner, Vec2f(left_corner[0] + reach, top));
        count = append_curve(points, count, Vec2f(right_corner[0] - reach, top), right_corner, Vec2f(right_corner[0] + up[0] * reach, right_corner[1] - up[1] * reach));
        points[count++] = Vec2f(x1, base);
    }
    if (flip)
    {
        for (uint32_t index = 0; index < count; ++index)
        {
            points[index][1] = top + base - points[index][1];
        }
        std::reverse(points, points + count);
    }
    return count;
}

} // namespace oryx
