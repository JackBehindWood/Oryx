#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Math/Vector2.h"

namespace oryx
{

// A view is a rectangle of the window that one client draws in and reads input for, with its own origin; the main view defaults to the whole window.
using ViewId = uint32_t;

constexpr ViewId k_main_view = 0;
constexpr uint32_t k_max_views = 8;

// Logical window points, origin top left.
struct ViewRegion
{
    Vec2f min{ 0.0f, 0.0f };
    Vec2f size{ 0.0f, 0.0f };
};

[[nodiscard]] inline bool contains(const ViewRegion& region, const Vec2f& point)
{
    return point[0] >= region.min[0] && point[1] >= region.min[1] && point[0] < region.min[0] + region.size[0] && point[1] < region.min[1] + region.size[1];
}

} // namespace oryx
