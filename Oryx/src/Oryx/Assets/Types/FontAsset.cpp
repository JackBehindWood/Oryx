#include "oxpch.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "Oryx/Assets/Types/FontAsset.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Core/Fnv.h"

namespace oryx
{

FontAsset FontAsset::from_bytes(std::vector<uint8_t> bytes)
{
    FontAsset font(std::move(bytes));
    const uint8_t* data = font.m_bytes.data();
    int32_t offset = font.m_bytes.empty() ? -1 : stbtt_GetFontOffsetForIndex(data, 0);
    if (offset < 0 || stbtt_InitFont(&font.m_info, data, offset) == 0)
    {
        throw Error("cannot parse font");
    }
    Fnv1a hash;
    hash.mix(data, font.m_bytes.size());
    font.m_content_hash = hash.value();
    return font;
}

bool FontAsset::has_glyph(uint32_t codepoint) const
{
    return stbtt_FindGlyphIndex(&m_info, static_cast<int32_t>(codepoint)) != 0;
}

FontMetrics FontAsset::metrics(float pixel_height) const
{
    float scale = stbtt_ScaleForPixelHeight(&m_info, pixel_height);
    int32_t ascent = 0;
    int32_t descent = 0;
    int32_t line_gap = 0;
    stbtt_GetFontVMetrics(&m_info, &ascent, &descent, &line_gap);
    return FontMetrics{ static_cast<float>(ascent) * scale, static_cast<float>(descent) * scale, static_cast<float>(line_gap) * scale };
}

float FontAsset::kerning(uint32_t left, uint32_t right, float pixel_height) const
{
    const float scale = stbtt_ScaleForPixelHeight(&m_info, pixel_height);
    return static_cast<float>(stbtt_GetCodepointKernAdvance(&m_info, static_cast<int32_t>(left), static_cast<int32_t>(right))) * scale;
}

GlyphBitmap FontAsset::rasterise(uint32_t codepoint, float pixel_height) const
{
    float scale = stbtt_ScaleForPixelHeight(&m_info, pixel_height);
    int32_t advance = 0;
    int32_t left_bearing = 0;
    stbtt_GetCodepointHMetrics(&m_info, static_cast<int32_t>(codepoint), &advance, &left_bearing);

    GlyphBitmap glyph;
    glyph.advance = static_cast<float>(advance) * scale;

    int32_t width = 0;
    int32_t height = 0;
    int32_t x_offset = 0;
    int32_t y_offset = 0;
    uint8_t* bitmap = stbtt_GetCodepointBitmap(&m_info, scale, scale, static_cast<int32_t>(codepoint), &width, &height, &x_offset, &y_offset);
    if (bitmap == nullptr)
    {
        return glyph;
    }
    glyph.width = width;
    glyph.height = height;
    glyph.x_offset = x_offset;
    glyph.y_offset = y_offset;
    glyph.coverage.assign(bitmap, bitmap + static_cast<size_t>(width) * static_cast<size_t>(height));
    stbtt_FreeBitmap(bitmap, nullptr);
    return glyph;
}

} // namespace oryx
