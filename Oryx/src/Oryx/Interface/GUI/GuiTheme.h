#pragma once

#include "Oryx/Interface/Canvas/ImTheme.h"

namespace oryx
{

// Which side of its control a field's label sits on. `Default` in a widget's options defers to the theme.
enum class LabelSide : uint8_t
{
    Default,
    Before,
    After
};

inline constexpr uint32_t k_palette_size = 8;

// The shared theme plus the roles only developer tooling needs. The role styles split what one `base` style used to cover, so a panel, a header, a field, a button, a tab, a scroll bar and a popup each get their own surface;
// a widget takes its role unless the call names a variant or a style. `base` stays the fallback for text and for code that predates the roles.
struct GuiTheme : ImTheme
{
    ImStyle panel;
    ImStyle header;
    ImStyle field;
    ImStyle button;
    ImStyle tab;
    // Track fill in `background`, thumb in `border`, hovered thumb in `hover`.
    ImStyle scroll;
    // Tooltips, menus and popups.
    ImStyle overlay;
    // Space between neighbouring widgets in a row or column.
    float spacing = 4.0f;
    // Label first, as in Unity and Godot forms; After gives ImGui's order.
    LabelSide label_side = LabelSide::Before;
    // Width of the label column; zero fits the text, a positive value aligns the controls of stacked fields.
    float label_width = 0.0f;
    // Pixels one wheel line scrolls.
    float scroll_line_px = 40.0f;
    float scrollbar_width = 8.0f;
    float tooltip_delay = 0.5f;
    // Covers everything beneath a modal.
    Colour modal_dim = { 0.0f, 0.0f, 0.0f, 0.5f };
    // Left padding of the children of a tree node.
    float indent = 16.0f;
    // Categorical colours for series, legends and tags: the Okabe-Ito set, distinguishable with the common colour-vision deficiencies.
    Colour palette[k_palette_size] = {
        { 0.337f, 0.706f, 0.914f, 1.0f },
        { 0.902f, 0.624f, 0.000f, 1.0f },
        { 0.000f, 0.620f, 0.451f, 1.0f },
        { 0.800f, 0.475f, 0.655f, 1.0f },
        { 0.941f, 0.894f, 0.259f, 1.0f },
        { 0.835f, 0.369f, 0.000f, 1.0f },
        { 0.000f, 0.447f, 0.698f, 1.0f },
        { 0.700f, 0.700f, 0.700f, 1.0f },
    };
};

static_assert(std::is_trivially_copyable_v<GuiTheme>);

// The few values a look is made of; make_gui_theme derives every role style from them, so a theme is a palette plus the rules below and nothing is tuned per widget.
//   role style   background / border                      state fills and marks
//   base         panel / border, no border drawn          every role: hover = shift(background, +hover_step), pressed = shift(background, -0.9 * hover_step)
//   panel        panel / border (drawn when panel_border) every role: text = text, accent = accent, on_accent = on_accent, selected = selected
//   header       header / border (drawn when panel_border) every role: radius = radius, text height 16, padding 8 x 2
//   field        field / field_border, border_width
//   button       button / field_border, border_width
//   tab          tab / border, none                       tab.selected = panel (the chosen tab joins its body)
//   scroll       panel / thumb, none                      scroll.hover = thumb shifted by twice the hover step
//   overlay      overlay / field_border, max(1, border_width)
// make_gui_theme also sets min_hit_size 18 and version 2.
struct GuiPalette
{
    Colour panel;
    Colour header;
    Colour field;
    Colour button;
    Colour tab;
    Colour overlay;
    Colour text;
    // Panel, header and tab edges.
    Colour border;
    // Field, button and overlay edges.
    Colour field_border;
    // The scroll bar thumb.
    Colour thumb;
    Colour accent;
    Colour on_accent;
    Colour selected;
    // Positive lightens a surface to show hover, negative darkens it.
    float hover_step = 0.07f;
    float radius = 3.0f;
    float border_width = 1.0f;
    // Draw the panel and header borders too (high contrast).
    bool panel_border = false;
};

// Graphite surfaces, one teal accent. The default.
[[nodiscard]] GuiPalette dark_gui_palette();
[[nodiscard]] GuiPalette light_gui_palette();
// Black and white with 2 px borders and a yellow accent; text and marks reach 7:1.
[[nodiscard]] GuiPalette high_contrast_gui_palette();
// Every role style from the palette (see GuiPalette for the rules); sizes are the unscaled defaults.
[[nodiscard]] GuiTheme make_gui_theme(const GuiPalette& palette);
// Recolours `theme` from the palette and keeps everything else: font, text heights, padding, spacing, scale and variants.
void apply_gui_palette(GuiTheme& theme, const GuiPalette& palette);

[[nodiscard]] GuiTheme dark_gui_theme();
[[nodiscard]] GuiTheme light_gui_theme();
[[nodiscard]] GuiTheme high_contrast_gui_theme();
// `theme` with every size (text, padding, radius, spacing, indent, scroll bar, hit area) multiplied by `factor`; the colours, font and palette stay.
[[nodiscard]] GuiTheme scale_gui_theme(const GuiTheme& theme, float factor);

// Setters over every role style, so one call keeps the roles consistent. Each is plain data in, plain data out; a context applies the result with set_theme.
// The accent of every role, with its on_accent and selected (see set_style_accent); the chosen tab keeps the panel colour it joins.
void set_accent(GuiTheme& theme, const Colour& accent);
void set_text_height(GuiTheme& theme, float height);
void set_corner_radius(GuiTheme& theme, float radius);
void set_text_colour(GuiTheme& theme, const Colour& text);

} // namespace oryx
