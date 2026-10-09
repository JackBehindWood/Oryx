#pragma once

#include "Oryx/Interface/Canvas/ImId.h"
#include "Oryx/Interface/Canvas/Rect.h"
#include "Oryx/Math/Colour.h"

namespace oryx
{

class Font;

// The look of one kind of widget; a variant ("primary", "danger") is another ImStyle under a name.
struct ImStyle
{
    Colour text = { 0.90f, 0.91f, 0.93f, 1.0f };
    Colour background = { 0.16f, 0.17f, 0.20f, 1.0f };
    Colour hover = { 0.22f, 0.24f, 0.29f, 1.0f };
    Colour pressed = { 0.12f, 0.13f, 0.16f, 1.0f };
    Colour border = { 0.32f, 0.34f, 0.40f, 1.0f };
    // Marks, fills of the chosen part of a control, and the focus ring; never a resting surface.
    Colour accent = { 0.30f, 0.56f, 0.95f, 1.0f };
    // A glyph or text drawn on an accent fill.
    Colour on_accent = { 0.05f, 0.06f, 0.08f, 1.0f };
    // The fill of a chosen row or tab; quieter than the accent so the text stays readable on it.
    Colour selected = { 0.18f, 0.30f, 0.52f, 1.0f };
    float radius = 4.0f;
    float border_width = 1.0f;
    float text_height = 16.0f;
    Insets padding = { 8.0f, 4.0f, 8.0f, 4.0f };
};

inline constexpr uint32_t k_max_style_variants = 8;

// The data shared by every immediate-mode theme; UiTheme and GuiTheme extend it with their own roles. Plain data so a loader, a builder or a live reload can produce it.
struct ImTheme
{
    // Not owned; must outlive the context using the theme.
    Font* font = nullptr;
    ImStyle base;
    ImStyle variants[k_max_style_variants];
    ImId variant_ids[k_max_style_variants];
    uint32_t variant_count = 0;
    // Items smaller than this on either side still get this much hit area (accessibility); zero disables.
    float min_hit_size = 0.0f;
    // Pointer travel in points before a held item counts as dragged.
    float drag_threshold = 4.0f;
    // Longest gap between two clicks on one item that makes the second a double click.
    float double_click_seconds = 0.35f;
    uint32_t version = 1;
};

// Colour from 0xRRGGBB, opaque.
[[nodiscard]] Colour colour_from_hex(uint32_t rgb);
// `colour` moved toward white (amount > 0) or black (amount < 0) by |amount| of the way, alpha kept. The one rule behind every hover and pressed fill.
[[nodiscard]] Colour shift_colour(const Colour& colour, float amount);
// WCAG contrast ratio of two opaque colours, 1 to 21.
[[nodiscard]] float contrast_ratio(const Colour& a, const Colour& b);
// Black or white, whichever has the higher contrast on `fill`.
[[nodiscard]] Colour on_colour(const Colour& fill);

// Sets `background` and derives the states from it: hover = shift_colour(background, hover_step), pressed = shift_colour(background, -0.9 * hover_step).
void set_style_surface(ImStyle& style, const Colour& background, float hover_step);
// Sets `accent` and derives the marks on it: on_accent = on_colour(accent), selected = background blended 35% toward the accent.
void set_style_accent(ImStyle& style, const Colour& accent);
// Multiplies the sizes (radius, border width, text height, padding) and leaves the colours.
void scale_style(ImStyle& style, float factor);
// Applies set_style_accent to the base style; the named variants keep their own accents.
void set_accent(ImTheme& theme, const Colour& accent);

// Adds the variant or replaces one of the same name. Throws Error when all slots are taken.
void add_style_variant(ImTheme& theme, std::string_view name, const ImStyle& style);
// The base style when no variant has this name or id.
[[nodiscard]] const ImStyle& style_for(const ImTheme& theme, ImId variant);
[[nodiscard]] const ImStyle& style_for(const ImTheme& theme, std::string_view variant);

} // namespace oryx
