#include "oxpch.h"
#include "Oryx/Interface/Canvas/ImTheme.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

float linear(float channel)
{
    return channel <= 0.04045f ? channel / 12.92f : std::pow((channel + 0.055f) / 1.055f, 2.4f);
}

float luminance(const Colour& colour)
{
    return 0.2126f * linear(colour.r) + 0.7152f * linear(colour.g) + 0.0722f * linear(colour.b);
}

} // namespace

Colour colour_from_hex(uint32_t rgb)
{
    return { static_cast<float>((rgb >> 16) & 0xFFu) / 255.0f, static_cast<float>((rgb >> 8) & 0xFFu) / 255.0f, static_cast<float>(rgb & 0xFFu) / 255.0f, 1.0f };
}

Colour shift_colour(const Colour& colour, float amount)
{
    const float target = amount >= 0.0f ? 1.0f : 0.0f;
    const float t = math::abs(amount);
    return { math::lerp(colour.r, target, t), math::lerp(colour.g, target, t), math::lerp(colour.b, target, t), colour.a };
}

float contrast_ratio(const Colour& a, const Colour& b)
{
    const float la = luminance(a);
    const float lb = luminance(b);
    return (math::max(la, lb) + 0.05f) / (math::min(la, lb) + 0.05f);
}

Colour on_colour(const Colour& fill)
{
    const Colour white = { 1.0f, 1.0f, 1.0f, 1.0f };
    const Colour black = { 0.0f, 0.0f, 0.0f, 1.0f };
    return contrast_ratio(fill, white) >= contrast_ratio(fill, black) ? white : black;
}

void set_style_surface(ImStyle& style, const Colour& background, float hover_step)
{
    style.background = background;
    style.hover = shift_colour(background, hover_step);
    style.pressed = shift_colour(background, -hover_step * 0.9f);
}

void set_style_accent(ImStyle& style, const Colour& accent)
{
    style.accent = accent;
    style.on_accent = on_colour(accent);
    style.selected = lerp(style.background, accent, 0.35f);
}

void scale_style(ImStyle& style, float factor)
{
    style.radius *= factor;
    style.border_width = style.border_width > 0.0f ? math::max(1.0f, style.border_width * factor) : 0.0f;
    style.text_height *= factor;
    style.padding = { style.padding.left * factor, style.padding.top * factor, style.padding.right * factor, style.padding.bottom * factor };
}

void set_accent(ImTheme& theme, const Colour& accent)
{
    set_style_accent(theme.base, accent);
}

void add_style_variant(ImTheme& theme, std::string_view name, const ImStyle& style)
{
    const ImId id = make_im_id(name);
    for (uint32_t index = 0; index < theme.variant_count; ++index)
    {
        if (theme.variant_ids[index] == id)
        {
            theme.variants[index] = style;
            return;
        }
    }
    if (theme.variant_count == k_max_style_variants)
    {
        throw Error("ImTheme has no free style variant slot", "at most k_max_style_variants variants");
    }
    theme.variant_ids[theme.variant_count] = id;
    theme.variants[theme.variant_count] = style;
    ++theme.variant_count;
}

const ImStyle& style_for(const ImTheme& theme, ImId variant)
{
    for (uint32_t index = 0; index < theme.variant_count; ++index)
    {
        if (theme.variant_ids[index] == variant)
        {
            return theme.variants[index];
        }
    }
    return theme.base;
}

const ImStyle& style_for(const ImTheme& theme, std::string_view variant)
{
    return style_for(theme, make_im_id(variant));
}

} // namespace oryx
