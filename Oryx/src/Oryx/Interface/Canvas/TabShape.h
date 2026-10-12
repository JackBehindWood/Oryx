#pragma once

#include "Oryx/Interface/Canvas/DrawList.h"

namespace oryx
{

// The outline of a tab drawn as a trapezoid on `rect`: the base is the rect's bottom edge at full width, the sides lean in by `slope` points over the rect's height and meet the top edge through a curved shoulder of about `shoulder` points.
// `flip` puts the base on top, for tabs along the bottom of a strip. Points run clockwise from the base's start to its end and the polygon is convex; the base edge itself is the last-to-first closing edge, so an open outline of the same points omits it.
// Returns the point count (always at most k_max_polygon_points); a zero or negative slope gives the four corners of the rect.
uint32_t tab_outline(const Rect& rect, float slope, float shoulder, bool flip, Vec2f (&points)[k_max_polygon_points]);

} // namespace oryx
