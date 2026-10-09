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

// Graphite surfaces, one teal accent. The default.
[[nodiscard]] GuiTheme dark_gui_theme();
[[nodiscard]] GuiTheme light_gui_theme();
// Black and white with 2 px borders and a yellow accent; text and marks reach 7:1.
[[nodiscard]] GuiTheme high_contrast_gui_theme();
// `theme` with every size (text, padding, radius, spacing, indent, scroll bar, hit area) multiplied by `factor`; the colours, font and palette stay.
[[nodiscard]] GuiTheme scale_gui_theme(const GuiTheme& theme, float factor);
// WCAG contrast ratio of two opaque colours, 1 to 21.
[[nodiscard]] float contrast_ratio(const Colour& a, const Colour& b);

} // namespace oryx
