#pragma once

#include "Oryx/Interface/Canvas/Id.h"
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
    Colour accent = { 0.30f, 0.56f, 0.95f, 1.0f };
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
    Id variant_ids[k_max_style_variants];
    uint32_t variant_count = 0;
    // Items smaller than this on either side still get this much hit area (accessibility); zero disables.
    float min_hit_size = 0.0f;
    uint32_t version = 1;
};

// Adds the variant or replaces one of the same name. Throws Error when all slots are taken.
void add_style_variant(ImTheme& theme, std::string_view name, const ImStyle& style);
// The base style when no variant has this name or id.
[[nodiscard]] const ImStyle& style_for(const ImTheme& theme, Id variant);
[[nodiscard]] const ImStyle& style_for(const ImTheme& theme, std::string_view variant);

} // namespace oryx
