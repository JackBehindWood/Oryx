#include "oxpch.h"
#include "Oryx/Text/BuiltinFontSource.h"

#include "Oryx/Core/Error.h"
#include "stb_truetype.h"

namespace oryx
{

namespace
{

constexpr uint32_t MIN_SIDE = 64;
constexpr uint32_t MAX_SIDE = 4096;

struct Rasterised
{
    uint32_t codepoint = 0;
    float advance = 0.0f;
    int32_t width = 0;
    int32_t height = 0;
    int32_t x_offset = 0;
    int32_t y_offset = 0;
    std::vector<uint8_t> coverage;
    uint32_t x = 0;
    uint32_t y = 0;
};

bool place(std::vector<Rasterised>& glyphs, uint32_t side, uint32_t padding)
{
    uint32_t x = padding;
    uint32_t y = padding;
    uint32_t row_height = 0;
    for (Rasterised& glyph : glyphs)
    {
        if (glyph.width <= 0)
        {
            continue;
        }
        const uint32_t width = static_cast<uint32_t>(glyph.width);
        const uint32_t height = static_cast<uint32_t>(glyph.height);
        if (x + width + padding > side)
        {
            x = padding;
            y += row_height + padding;
            row_height = 0;
        }
        if (width + 2 * padding > side || y + height + padding > side)
        {
            return false;
        }
        glyph.x = x;
        glyph.y = y;
        x += width + padding;
        row_height = std::max(row_height, height);
    }
    return true;
}

} // namespace

GlyphAtlasData BuiltinFontSource::bake(const GlyphAtlasDesc& desc)
{
    validate_glyph_atlas_desc(desc);
    stbtt_fontinfo info;
    if (stbtt_InitFont(&info, BUILTIN_FONT_DATA, 0) == 0)
    {
        throw Error("cannot parse the builtin font");
    }
    const float scale = stbtt_ScaleForPixelHeight(&info, desc.pixel_height);

    std::vector<Rasterised> glyphs;
    for (uint32_t codepoint : glyph_atlas_codepoints(desc))
    {
        const int32_t index = stbtt_FindGlyphIndex(&info, static_cast<int32_t>(codepoint));
        if (index == 0)
        {
            continue;
        }
        Rasterised& glyph = glyphs.emplace_back();
        glyph.codepoint = codepoint;
        int32_t advance = 0;
        int32_t left_bearing = 0;
        stbtt_GetGlyphHMetrics(&info, index, &advance, &left_bearing);
        glyph.advance = static_cast<float>(advance) * scale;
        uint8_t* bitmap = stbtt_GetGlyphBitmap(&info, scale, scale, index, &glyph.width, &glyph.height, &glyph.x_offset, &glyph.y_offset);
        if (bitmap != nullptr)
        {
            glyph.coverage.assign(bitmap, bitmap + static_cast<size_t>(glyph.width) * static_cast<size_t>(glyph.height));
            stbtt_FreeBitmap(bitmap, nullptr);
        }
    }

    std::stable_sort(glyphs.begin(), glyphs.end(), [](const Rasterised& a, const Rasterised& b) { return a.height > b.height; });
    uint32_t side = MIN_SIDE;
    while (!place(glyphs, side, desc.padding))
    {
        if (side >= MAX_SIDE)
        {
            throw Error("Builtin font does not fit", "reduce the pixel height");
        }
        side *= 2;
    }

    std::vector<uint8_t> pixels(static_cast<size_t>(side) * side, 0);
    std::vector<GlyphEntry> entries;
    const float inverse = 1.0f / static_cast<float>(side);
    for (const Rasterised& source : glyphs)
    {
        Glyph glyph;
        glyph.advance = source.advance;
        if (source.width > 0 && source.height > 0)
        {
            const size_t width = static_cast<size_t>(source.width);
            for (int32_t row = 0; row < source.height; ++row)
            {
                std::memcpy(&pixels[static_cast<size_t>(source.y + row) * side + source.x], &source.coverage[static_cast<size_t>(row) * width], width);
            }
            glyph.uv_min = Vec2f(static_cast<float>(source.x) * inverse, static_cast<float>(source.y) * inverse);
            glyph.uv_max = Vec2f(static_cast<float>(source.x + source.width) * inverse, static_cast<float>(source.y + source.height) * inverse);
            glyph.size = Vec2f(static_cast<float>(source.width), static_cast<float>(source.height));
            glyph.bearing = Vec2f(static_cast<float>(source.x_offset), -static_cast<float>(source.y_offset + source.height));
        }
        entries.push_back({ source.codepoint, glyph });
    }
    std::sort(entries.begin(), entries.end(), [](const GlyphEntry& a, const GlyphEntry& b) { return a.codepoint < b.codepoint; });

    int32_t ascent = 0;
    int32_t descent = 0;
    int32_t line_gap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    const float ascent_px = static_cast<float>(ascent) * scale;
    const float line_height = ascent_px - static_cast<float>(descent) * scale + static_cast<float>(line_gap) * scale;
    return GlyphAtlasData(desc, side, std::move(pixels), std::move(entries), {}, ascent_px, line_height);
}

} // namespace oryx
