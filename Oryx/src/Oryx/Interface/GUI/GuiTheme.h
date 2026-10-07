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

// The shared theme plus the roles only developer tooling needs.
struct GuiTheme : ImTheme
{
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

} // namespace oryx
