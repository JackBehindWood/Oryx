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

enum class TabShape : uint8_t
{
    // A plain rectangle, the look before tab styles.
    Flat,
    // A trapezoid whose sides lean in, with curved shoulders; neighbours overlap and the chosen tab stands taller.
    Sloped,
    // A rectangle with rounded top corners (bottom ones for tabs along the bottom of a strip), no overlap.
    Rounded
};

// The look of a tab, shared by the dock's strips and the tab_bar widget; named by what it draws. Lengths are points at a 16 point tab text and scale with the tab text height.
struct TabStyle
{
    TabShape shape = TabShape::Sloped;
    // How far each top corner leans in from the base.
    float slope = 8.0f;
    // The radius of the curve where a side meets the top edge.
    float shoulder = 5.0f;
    // How far neighbouring tabs overlap in a dock strip.
    float overlap = 6.0f;
    // How much taller the chosen tab stands than the others.
    float selected_raise = 3.0f;
    // Rounded tabs: the radius of the outer corners.
    float radius = 0.0f;

    [[nodiscard]] static constexpr TabStyle sloped() { return TabStyle{}; }
    [[nodiscard]] static constexpr TabStyle flat() { return TabStyle{ TabShape::Flat, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }; }
    [[nodiscard]] static constexpr TabStyle rounded() { return TabStyle{ TabShape::Rounded, 0.0f, 0.0f, 0.0f, 0.0f, 8.0f }; }
};

static_assert(std::is_trivially_copyable_v<TabStyle> && std::is_standard_layout_v<TabStyle>);

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
    TabStyle tab_style = TabStyle::sloped();
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
    // The reach of the one drop shadow under popups, menus, tooltips, toasts and dialogs; zero turns it off.
    float elevation = 8.0f;
    // The 4/8-point steps the chrome spaces by besides `spacing` (4): xs for tight strips, md for rows of controls.
    float space_xs = 2.0f;
    float space_md = 8.0f;
    // One height for every bar: dock tab strips and toolbars, the menu bar, the status bar and table headers.
    float bar_height = 24.0f;
    // One height for every row of a table, list or decision table.
    float row_height = 22.0f;
    // Type scale as multiples of the role's text height (the body size): title, body (1) and caption.
    float title_scale = 1.125f;
    float caption_scale = 0.8125f;
    // Secondary text on a panel: legible (4.5:1, 7:1 with panel_border) but quieter than `text`.
    Colour caption_colour;
    // Log line text: debug dimmed, info plain, warning amber, error red. Each is legible on field.background; the level letter still names the level.
    Colour log_debug;
    Colour log_info;
    Colour log_warning;
    Colour log_error;
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
//   log roles    debug/info/warning/error pulled toward text until they reach 4.5:1 (7:1 with panel_border) on field
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
