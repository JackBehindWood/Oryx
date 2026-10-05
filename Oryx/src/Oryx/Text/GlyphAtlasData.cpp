#include "oxpch.h"
#include "Oryx/Text/GlyphAtlasData.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Core/Utf8.h"

namespace oryx
{

void validate_glyph_atlas_desc(const GlyphAtlasDesc& desc)
{
    if (!(desc.pixel_height > 0.0f) || desc.pixel_height > GLYPH_ATLAS_MAX_PIXEL_HEIGHT)
    {
        throw Error("GlyphAtlas pixel height is out of range", std::to_string(desc.pixel_height));
    }
    (void)glyph_atlas_codepoints(desc);
}

std::vector<uint32_t> glyph_atlas_codepoints(const GlyphAtlasDesc& desc)
{
    std::vector<uint32_t> codepoints;
    for (const CodepointRange& range : desc.ranges)
    {
        if (range.first > range.last || range.last > 0x10FFFF)
        {
            throw Error("GlyphAtlas range is invalid", std::to_string(range.first) + ".." + std::to_string(range.last));
        }
        if (range.last - range.first >= GLYPH_ATLAS_MAX_GLYPHS)
        {
            throw Error("GlyphAtlas has too many glyphs", std::to_string(GLYPH_ATLAS_MAX_GLYPHS) + " at most");
        }
        for (uint32_t codepoint = range.first; codepoint <= range.last; ++codepoint)
        {
            codepoints.push_back(codepoint);
        }
    }
    std::sort(codepoints.begin(), codepoints.end());
    codepoints.erase(std::unique(codepoints.begin(), codepoints.end()), codepoints.end());
    if (codepoints.size() > GLYPH_ATLAS_MAX_GLYPHS)
    {
        throw Error("GlyphAtlas has too many glyphs", std::to_string(GLYPH_ATLAS_MAX_GLYPHS) + " at most");
    }
    return codepoints;
}

GlyphAtlasData::GlyphAtlasData(GlyphAtlasDesc desc, uint32_t side, std::vector<uint8_t> pixels, std::vector<GlyphEntry> glyphs, std::vector<KerningPair> kerning, float ascent, float line_height)
    : m_desc(std::move(desc))
    , m_pixels(std::move(pixels))
    , m_glyphs(std::move(glyphs))
    , m_kerning(std::move(kerning))
    , m_ascent(ascent)
    , m_line_height(line_height)
    , m_side(side)
{
    if (m_pixels.size() != static_cast<size_t>(m_side) * m_side)
    {
        throw Error("GlyphAtlasData pixels do not match its side");
    }
}

const Glyph* GlyphAtlasData::find(uint32_t codepoint) const
{
    const std::vector<GlyphEntry>::const_iterator it = std::lower_bound(m_glyphs.begin(), m_glyphs.end(), codepoint, [](const GlyphEntry& entry, uint32_t value) { return entry.codepoint < value; });
    return (it != m_glyphs.end() && it->codepoint == codepoint) ? &it->glyph : nullptr;
}

const Glyph& GlyphAtlasData::glyph(uint32_t codepoint) const
{
    static const Glyph EMPTY;
    for (uint32_t candidate : { codepoint, UTF8_REPLACEMENT, static_cast<uint32_t>('?') })
    {
        if (const Glyph* found = find(candidate))
        {
            return *found;
        }
    }
    return EMPTY;
}

float GlyphAtlasData::kerning(uint32_t left, uint32_t right) const
{
    const std::pair<uint32_t, uint32_t> key(left, right);
    const std::vector<KerningPair>::const_iterator it = std::lower_bound(m_kerning.begin(), m_kerning.end(), key, [](const KerningPair& pair, const std::pair<uint32_t, uint32_t>& value) { return std::pair<uint32_t, uint32_t>(pair.left, pair.right) < value; });
    return (it != m_kerning.end() && it->left == left && it->right == right) ? it->advance : 0.0f;
}

} // namespace oryx
