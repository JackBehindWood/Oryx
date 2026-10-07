#pragma once

#include "Oryx/Interface/Canvas/DrawList.h"

namespace oryx
{

// Small symbols drawn from lines, so they need no font or texture and stay crisp at any scale.
enum class Icon : uint8_t
{
    None,
    ChevronRight,
    ChevronDown,
    ChevronUp,
    ChevronLeft,
    Cross,
    Plus,
    Minus,
    Check
};

// Strokes `icon` centred in `box`. `size` is the glyph's extent in points (zero: half the smaller side of the box).
void draw_icon(DrawList& list, const Rect& box, Icon icon, const Colour& colour, float size = 0.0f, float thickness = 1.5f);

} // namespace oryx
