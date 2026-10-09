#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiTheme.h"

namespace oryx
{

namespace
{

ImStyle role(const GuiPalette& palette, const Colour& background, const Colour& border, float border_width)
{
    ImStyle style;
    set_style_surface(style, background, palette.hover_step);
    style.text = palette.text;
    style.border = border;
    style.accent = palette.accent;
    style.on_accent = palette.on_accent;
    style.selected = palette.selected;
    style.radius = palette.radius;
    style.border_width = border_width;
    style.text_height = 16.0f;
    style.padding = { 8.0f, 2.0f, 8.0f, 2.0f };
    return style;
}

// The role styles in one place, so setters and the palette applier cannot miss one.
constexpr ImStyle GuiTheme::* k_roles[] = { &GuiTheme::panel, &GuiTheme::header, &GuiTheme::field, &GuiTheme::button, &GuiTheme::tab, &GuiTheme::scroll, &GuiTheme::overlay };

template<typename Fn>
void each_style(GuiTheme& theme, Fn&& fn)
{
    fn(theme.base);
    for (ImStyle GuiTheme::* member : k_roles)
    {
        fn(theme.*member);
    }
}

Colour scroll_hover(const GuiPalette& palette)
{
    return shift_colour(palette.thumb, math::abs(palette.hover_step) * 2.0f * (palette.hover_step >= 0.0f ? 1.0f : -1.0f));
}

} // namespace

GuiTheme make_gui_theme(const GuiPalette& palette)
{
    GuiTheme theme;
    theme.version = 2;
    const float framed = palette.panel_border ? palette.border_width : 0.0f;
    theme.base = role(palette, palette.panel, palette.border, 0.0f);
    theme.panel = role(palette, palette.panel, palette.border, framed);
    theme.header = role(palette, palette.header, palette.border, framed);
    theme.field = role(palette, palette.field, palette.field_border, palette.border_width);
    theme.button = role(palette, palette.button, palette.field_border, palette.border_width);
    theme.tab = role(palette, palette.tab, palette.border, 0.0f);
    theme.tab.selected = palette.panel;
    theme.scroll = role(palette, palette.panel, palette.thumb, 0.0f);
    theme.scroll.hover = scroll_hover(palette);
    theme.overlay = role(palette, palette.overlay, palette.field_border, math::max(1.0f, palette.border_width));
    theme.min_hit_size = 18.0f;
    return theme;
}

void apply_gui_palette(GuiTheme& theme, const GuiPalette& palette)
{
    const GuiTheme fresh = make_gui_theme(palette);
    const auto recolour = [](ImStyle& style, const ImStyle& from) {
        style.text = from.text;
        style.background = from.background;
        style.hover = from.hover;
        style.pressed = from.pressed;
        style.border = from.border;
        style.accent = from.accent;
        style.on_accent = from.on_accent;
        style.selected = from.selected;
        style.radius = from.radius;
        style.border_width = from.border_width;
    };
    recolour(theme.base, fresh.base);
    for (ImStyle GuiTheme::* member : k_roles)
    {
        recolour(theme.*member, fresh.*member);
    }
}

GuiPalette dark_gui_palette()
{
    GuiPalette p;
    p.panel = colour_from_hex(0x242424);
    p.header = colour_from_hex(0x1B1B1B);
    p.field = colour_from_hex(0x121212);
    p.button = colour_from_hex(0x353535);
    p.tab = colour_from_hex(0x1B1B1B);
    p.overlay = colour_from_hex(0x2B2B2B);
    p.text = colour_from_hex(0xE4E4E4);
    p.border = colour_from_hex(0x333333);
    p.field_border = colour_from_hex(0x454545);
    p.thumb = colour_from_hex(0x555555);
    p.accent = colour_from_hex(0x3FB7A6);
    p.on_accent = colour_from_hex(0x0C1210);
    p.selected = colour_from_hex(0x1F5F57);
    p.hover_step = 0.07f;
    p.radius = 3.0f;
    p.border_width = 1.0f;
    p.panel_border = false;
    return p;
}

GuiPalette light_gui_palette()
{
    GuiPalette p;
    p.panel = colour_from_hex(0xF0F0F1);
    p.header = colour_from_hex(0xE2E2E4);
    p.field = colour_from_hex(0xFFFFFF);
    p.button = colour_from_hex(0xE6E6E8);
    p.tab = colour_from_hex(0xE2E2E4);
    p.overlay = colour_from_hex(0xFFFFFF);
    p.text = colour_from_hex(0x1B1B1D);
    p.border = colour_from_hex(0xC8C8CC);
    p.field_border = colour_from_hex(0x9A9AA0);
    p.thumb = colour_from_hex(0xB0B0B6);
    p.accent = colour_from_hex(0x0B7A6C);
    p.on_accent = colour_from_hex(0xFFFFFF);
    p.selected = colour_from_hex(0xBFE5DF);
    p.hover_step = -0.06f;
    p.radius = 3.0f;
    p.border_width = 1.0f;
    p.panel_border = false;
    return p;
}

GuiPalette high_contrast_gui_palette()
{
    GuiPalette p;
    const Colour black = colour_from_hex(0x000000);
    const Colour white = colour_from_hex(0xFFFFFF);
    p.panel = black;
    p.header = black;
    p.field = black;
    p.button = black;
    p.tab = black;
    p.overlay = black;
    p.text = white;
    p.border = white;
    p.field_border = white;
    p.thumb = white;
    p.accent = colour_from_hex(0xFFD400);
    p.on_accent = black;
    p.selected = colour_from_hex(0x1A4C9E);
    p.hover_step = 0.25f;
    p.radius = 2.0f;
    p.border_width = 2.0f;
    p.panel_border = true;
    return p;
}

GuiTheme dark_gui_theme() { return make_gui_theme(dark_gui_palette()); }
GuiTheme light_gui_theme() { return make_gui_theme(light_gui_palette()); }
GuiTheme high_contrast_gui_theme() { return make_gui_theme(high_contrast_gui_palette()); }

GuiTheme scale_gui_theme(const GuiTheme& theme, float factor)
{
    GuiTheme scaled = theme;
    each_style(scaled, [factor](ImStyle& style) { scale_style(style, factor); });
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

void set_accent(GuiTheme& theme, const Colour& accent)
{
    each_style(theme, [&accent](ImStyle& style) { set_style_accent(style, accent); });
    theme.tab.selected = theme.panel.background;
}

void set_text_height(GuiTheme& theme, float height)
{
    each_style(theme, [height](ImStyle& style) { style.text_height = height; });
}

void set_corner_radius(GuiTheme& theme, float radius)
{
    each_style(theme, [radius](ImStyle& style) { style.radius = radius; });
}

void set_text_colour(GuiTheme& theme, const Colour& text)
{
    each_style(theme, [&text](ImStyle& style) { style.text = text; });
}

} // namespace oryx
