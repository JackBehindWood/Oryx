#pragma once

#include "Oryx/Math/Vector.h"

namespace oryx
{

// Logical points, origin top-left, y down.
struct Rect
{
    Vec2f min{ 0.0f, 0.0f };
    Vec2f size{ 0.0f, 0.0f };
};

struct Insets
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct CornerRadius
{
    float top_left = 0.0f;
    float top_right = 0.0f;
    float bottom_right = 0.0f;
    float bottom_left = 0.0f;
};

[[nodiscard]] constexpr Insets uniform_insets(float value) { return { value, value, value, value }; }
[[nodiscard]] constexpr CornerRadius uniform_radius(float value) { return { value, value, value, value }; }

[[nodiscard]] inline Vec2f rect_max(const Rect& rect) { return Vec2f(rect.min[0] + rect.size[0], rect.min[1] + rect.size[1]); }
[[nodiscard]] inline Vec2f rect_centre(const Rect& rect) { return Vec2f(rect.min[0] + rect.size[0] * 0.5f, rect.min[1] + rect.size[1] * 0.5f); }
[[nodiscard]] inline bool is_empty(const Rect& rect) { return !(rect.size[0] > 0.0f) || !(rect.size[1] > 0.0f); }
[[nodiscard]] inline bool operator==(const Rect& a, const Rect& b) { return a.min == b.min && a.size == b.size; }
[[nodiscard]] inline bool operator==(const CornerRadius& a, const CornerRadius& b)
{
    return a.top_left == b.top_left && a.top_right == b.top_right && a.bottom_right == b.bottom_right && a.bottom_left == b.bottom_left;
}

// Half-open: the min edge is inside, the max edge is not.
[[nodiscard]] bool contains(const Rect& rect, const Vec2f& point);
// An empty result keeps the overlap's min and a zero size.
[[nodiscard]] Rect intersect(const Rect& a, const Rect& b);
[[nodiscard]] bool overlaps(const Rect& a, const Rect& b);
// Shrinks by the insets; the size never goes negative.
[[nodiscard]] Rect inset(const Rect& rect, const Insets& insets);
[[nodiscard]] Rect expand(const Rect& rect, const Insets& insets);
// Grows around the centre so neither side is below `minimum`.
[[nodiscard]] Rect at_least(const Rect& rect, const Vec2f& minimum);
// Radii limited so adjacent corners never overlap on a rect of this size.
[[nodiscard]] CornerRadius clamp_radius(const CornerRadius& radius, const Vec2f& size);
[[nodiscard]] bool is_square(const CornerRadius& radius);
// Rounds to whole device pixels at `scale` device pixels per point.
[[nodiscard]] float snap_to_pixel(float value, float scale);
[[nodiscard]] Rect snap_to_pixels(const Rect& rect, float scale);

} // namespace oryx
