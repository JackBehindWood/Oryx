#pragma once

#include "Oryx/Core/Utf8.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Text/GlyphAtlasData.h"

namespace oryx
{

struct TextStyle
{
    // The size the glyphs are baked at; crisp when drawn at scale 1 through a pixel-unit camera.
    float pixel_height = 16.0f;
    Colour colour = { 1.0f, 1.0f, 1.0f, 1.0f };
    // A world-space multiplier applied after baking.
    float scale = 1.0f;
};

struct TextExtent
{
    float width = 0.0f;
    float height = 0.0f;
};

// The one text layout, shared by measuring and drawing. `visit(const Glyph&, const Vec2f& pen)` runs for every drawable glyph with the pen on the baseline, relative to the
// start of the text (y up, so later lines have negative y). '\n' starts a line, '\r' is skipped, '\t' advances four spaces; kerning applies between neighbouring codepoints.
template<typename Visitor>
TextExtent layout_text(const GlyphAtlasData& atlas, std::string_view text, float scale, Visitor&& visit)
{
    const float line_height = atlas.line_height() * scale;
    Vec2f pen(0.0f, 0.0f);
    TextExtent extent;
    uint32_t lines = text.empty() ? 0 : 1;
    uint32_t previous = 0;
    size_t index = 0;
    while (index < text.size())
    {
        const uint32_t codepoint = decode_utf8(text, index);
        if (codepoint == '\n')
        {
            pen = Vec2f(0.0f, pen[1] - line_height);
            previous = 0;
            ++lines;
            continue;
        }
        if (codepoint == '\r')
        {
            continue;
        }
        if (codepoint == '\t')
        {
            pen[0] += atlas.glyph(' ').advance * scale * 4.0f;
            previous = 0;
        }
        else
        {
            if (previous != 0)
            {
                pen[0] += atlas.kerning(previous, codepoint) * scale;
            }
            const Glyph& glyph = atlas.glyph(codepoint);
            visit(glyph, pen);
            pen[0] += glyph.advance * scale;
            previous = codepoint;
        }
        extent.width = std::max(extent.width, pen[0]);
    }
    extent.height = static_cast<float>(lines) * line_height;
    return extent;
}

} // namespace oryx
