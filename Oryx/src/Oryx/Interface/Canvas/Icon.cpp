#include "oxpch.h"
#include "Oryx/Interface/Canvas/Icon.h"

namespace oryx
{

void draw_icon(DrawList& list, const Rect& box, Icon icon, const Colour& colour, float size, float thickness)
{
    const float side = size > 0.0f ? size : math::min(box.size[0], box.size[1]) * 0.5f;
    if (icon == Icon::None || side <= 0.0f)
    {
        return;
    }
    const float x = box.min[0] + box.size[0] * 0.5f;
    const float y = box.min[1] + box.size[1] * 0.5f;
    const float half = side * 0.5f;
    const float quarter = side * 0.25f;
    const auto stroke = [&](float x0, float y0, float x1, float y1) { list.add_line({ x + x0, y + y0 }, { x + x1, y + y1 }, thickness, colour); };
    switch (icon)
    {
        case Icon::None: break;
        case Icon::ChevronRight:
            stroke(-quarter, -half, quarter, 0.0f);
            stroke(quarter, 0.0f, -quarter, half);
            break;
        case Icon::ChevronLeft:
            stroke(quarter, -half, -quarter, 0.0f);
            stroke(-quarter, 0.0f, quarter, half);
            break;
        case Icon::ChevronDown:
            stroke(-half, -quarter, 0.0f, quarter);
            stroke(0.0f, quarter, half, -quarter);
            break;
        case Icon::ChevronUp:
            stroke(-half, quarter, 0.0f, -quarter);
            stroke(0.0f, -quarter, half, quarter);
            break;
        case Icon::Cross:
            stroke(-half * 0.8f, -half * 0.8f, half * 0.8f, half * 0.8f);
            stroke(-half * 0.8f, half * 0.8f, half * 0.8f, -half * 0.8f);
            break;
        case Icon::Plus:
            stroke(-half, 0.0f, half, 0.0f);
            stroke(0.0f, -half, 0.0f, half);
            break;
        case Icon::Minus: stroke(-half, 0.0f, half, 0.0f); break;
        case Icon::Check:
            stroke(-half, 0.0f, -quarter * 0.5f, quarter);
            stroke(-quarter * 0.5f, quarter, half, -quarter);
            break;
    }
}

} // namespace oryx
