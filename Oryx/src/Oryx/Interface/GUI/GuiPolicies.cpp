#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiPolicies.h"

namespace oryx
{

std::string_view format_number(const NumberFormat& format, float value, FrameArena& arena)
{
    if (!std::isfinite(value))
    {
        return arena.format("-");
    }
    const int32_t precision = format.precision;
    switch (format.style)
    {
        case NumberStyle::Fixed: return arena.format("%.*f", precision, static_cast<double>(value));
        case NumberStyle::Percent: return arena.format("%.*f%%", precision, static_cast<double>(value) * 100.0);
        case NumberStyle::Integer: return arena.format("%.0f", static_cast<double>(value));
        case NumberStyle::General: break;
    }
    return arena.format("%.*g", precision, static_cast<double>(value));
}

Colour evaluate(const ColourScale& scale, float value)
{
    if (!std::isfinite(value))
    {
        return scale.no_data;
    }
    const float span = scale.max - scale.min;
    const float t = span > math::EPSILON<float> ? math::clamp((value - scale.min) / span, 0.0f, 1.0f) : 0.0f;
    return t < 0.5f ? lerp(scale.low, scale.mid, t * 2.0f) : lerp(scale.mid, scale.high, (t - 0.5f) * 2.0f);
}

ColourScale sequential_scale(float min, float max)
{
    ColourScale scale;
    scale.min = min;
    scale.max = max;
    return scale;
}

ColourScale diverging_scale(float min, float max)
{
    ColourScale scale;
    scale.low = { 0.17f, 0.38f, 0.69f, 1.0f };
    scale.mid = { 0.96f, 0.96f, 0.96f, 1.0f };
    scale.high = { 0.90f, 0.45f, 0.10f, 1.0f };
    scale.min = min;
    scale.max = max;
    return scale;
}

Vec2f to_screen(const PlotArea& area, float x, float y)
{
    const float x_span = area.x_max - area.x_min;
    const float y_span = area.y_max - area.y_min;
    const float tx = x_span > math::EPSILON<float> ? (x - area.x_min) / x_span : 0.0f;
    const float ty = y_span > math::EPSILON<float> ? (y - area.y_min) / y_span : 0.0f;
    return Vec2f(area.rect.min[0] + tx * area.rect.size[0], area.rect.min[1] + (1.0f - ty) * area.rect.size[1]);
}

float index_at(const PlotArea& area, float screen_x)
{
    const float width = area.rect.size[0];
    const float t = width > math::EPSILON<float> ? (screen_x - area.rect.min[0]) / width : 0.0f;
    return area.x_min + t * (area.x_max - area.x_min);
}

const Colour& palette_colour(const GuiTheme& theme, uint32_t index)
{
    return theme.palette[index % k_palette_size];
}

} // namespace oryx
