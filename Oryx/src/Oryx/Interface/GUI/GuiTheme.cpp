#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiTheme.h"

namespace oryx
{

namespace
{

Colour rgb(uint32_t hex)
{
    return { static_cast<float>((hex >> 16) & 0xFFu) / 255.0f, static_cast<float>((hex >> 8) & 0xFFu) / 255.0f, static_cast<float>(hex & 0xFFu) / 255.0f, 1.0f };
}

Colour shift(const Colour& colour, float amount)
{
    const float target = amount >= 0.0f ? 1.0f : 0.0f;
    const float t = math::abs(amount);
    return { math::lerp(colour.r, target, t), math::lerp(colour.g, target, t), math::lerp(colour.b, target, t), colour.a };
}

float linear(float channel)
{
    return channel <= 0.04045f ? channel / 12.92f : std::pow((channel + 0.055f) / 1.055f, 2.4f);
}

float luminance(const Colour& colour)
{
    return 0.2126f * linear(colour.r) + 0.7152f * linear(colour.g) + 0.0722f * linear(colour.b);
}

// What differs between the three looks; every role is derived from these.
struct Seed
{
    uint32_t panel;
    uint32_t header;
    uint32_t field;
    uint32_t button;
    uint32_t tab;
    uint32_t overlay;
    uint32_t text;
    uint32_t border;
    uint32_t field_border;
    uint32_t thumb;
    uint32_t accent;
    uint32_t on_accent;
    uint32_t selected;
    // Positive lightens a surface to show hover, negative darkens it.
    float hover_step;
    float radius;
    float border_width;
    bool panel_border;
};

ImStyle role(const Seed& seed, uint32_t background, uint32_t border, float border_width)
{
    ImStyle style;
    const Colour fill = rgb(background);
    style.text = rgb(seed.text);
    style.background = fill;
    style.hover = shift(fill, seed.hover_step);
    style.pressed = shift(fill, -seed.hover_step * 0.9f);
    style.border = rgb(border);
    style.accent = rgb(seed.accent);
    style.on_accent = rgb(seed.on_accent);
    style.selected = rgb(seed.selected);
    style.radius = seed.radius;
    style.border_width = border_width;
    style.text_height = 16.0f;
    style.padding = { 8.0f, 2.0f, 8.0f, 2.0f };
    return style;
}

GuiTheme build(const Seed& seed)
{
    GuiTheme theme;
    theme.version = 2;
    theme.base = role(seed, seed.panel, seed.border, 0.0f);
    theme.panel = role(seed, seed.panel, seed.border, seed.panel_border ? seed.border_width : 0.0f);
    theme.header = role(seed, seed.header, seed.border, seed.panel_border ? seed.border_width : 0.0f);
    theme.field = role(seed, seed.field, seed.field_border, seed.border_width);
    theme.button = role(seed, seed.button, seed.field_border, seed.border_width);
    theme.tab = role(seed, seed.tab, seed.border, 0.0f);
    theme.tab.selected = rgb(seed.panel);
    theme.scroll = role(seed, seed.panel, seed.thumb, 0.0f);
    theme.scroll.hover = shift(rgb(seed.thumb), math::abs(seed.hover_step) * 2.0f * (seed.hover_step >= 0.0f ? 1.0f : -1.0f));
    theme.overlay = role(seed, seed.overlay, seed.field_border, math::max(1.0f, seed.border_width));
    theme.min_hit_size = 18.0f;
    theme.dock_preview = theme.panel.accent;
    theme.dock_preview.a = 0.28f;
    return theme;
}

void scale_style(ImStyle& style, float factor)
{
    style.radius *= factor;
    style.border_width = style.border_width > 0.0f ? math::max(1.0f, style.border_width * factor) : 0.0f;
    style.text_height *= factor;
    style.padding = { style.padding.left * factor, style.padding.top * factor, style.padding.right * factor, style.padding.bottom * factor };
}

} // namespace

GuiTheme dark_gui_theme()
{
    return build({ 0x242424, 0x1B1B1B, 0x121212, 0x353535, 0x1B1B1B, 0x2B2B2B, 0xE4E4E4, 0x333333, 0x454545, 0x555555, 0x3FB7A6, 0x0C1210, 0x1F5F57, 0.07f, 3.0f, 1.0f, false });
}

GuiTheme light_gui_theme()
{
    return build({ 0xF0F0F1, 0xE2E2E4, 0xFFFFFF, 0xE6E6E8, 0xE2E2E4, 0xFFFFFF, 0x1B1B1D, 0xC8C8CC, 0x9A9AA0, 0xB0B0B6, 0x0B7A6C, 0xFFFFFF, 0xBFE5DF, -0.06f, 3.0f, 1.0f, false });
}

GuiTheme high_contrast_gui_theme()
{
    return build({ 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFD400, 0x000000, 0x1A4C9E, 0.25f, 2.0f, 2.0f, true });
}

GuiTheme scale_gui_theme(const GuiTheme& theme, float factor)
{
    GuiTheme scaled = theme;
    scale_style(scaled.base, factor);
    scale_style(scaled.panel, factor);
    scale_style(scaled.header, factor);
    scale_style(scaled.field, factor);
    scale_style(scaled.button, factor);
    scale_style(scaled.tab, factor);
    scale_style(scaled.scroll, factor);
    scale_style(scaled.overlay, factor);
    for (uint32_t index = 0; index < scaled.variant_count; ++index)
    {
        scale_style(scaled.variants[index], factor);
    }
    scaled.spacing *= factor;
    scaled.label_width *= factor;
    scaled.scroll_line_px *= factor;
    scaled.scrollbar_width *= factor;
    scaled.indent *= factor;
    scaled.min_hit_size *= factor;
    return scaled;
}

float contrast_ratio(const Colour& a, const Colour& b)
{
    const float la = luminance(a);
    const float lb = luminance(b);
    return (math::max(la, lb) + 0.05f) / (math::min(la, lb) + 0.05f);
}

} // namespace oryx
