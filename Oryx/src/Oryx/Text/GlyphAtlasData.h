#pragma once

#include "Oryx/Math/Vector2.h"

namespace oryx
{

struct CodepointRange
{
    uint32_t first = 0;
    uint32_t last = 0;
};

// Coverage stores glyph alpha; a signed-distance encoding is the recorded seam (Vertex2DText::px_range already carries its range).
enum class GlyphAtlasEncoding : uint8_t
{
    Coverage
};

struct GlyphAtlasDesc
{
    float pixel_height = 16.0f;
    // Basic Latin and the Latin-1 Supplement.
    std::vector<CodepointRange> ranges = { { 0x20, 0x7E }, { 0xA0, 0xFF } };
    uint32_t padding = 1;
    GlyphAtlasEncoding encoding = GlyphAtlasEncoding::Coverage;
};

// uv_min is the top-left of the glyph's atlas rectangle. bearing is the offset from the pen on the baseline to the quad's bottom-left corner, y up.
struct Glyph
{
    Vec2f uv_min = { 0.0f, 0.0f };
    Vec2f uv_max = { 0.0f, 0.0f };
    Vec2f size = { 0.0f, 0.0f };
    Vec2f bearing = { 0.0f, 0.0f };
    float advance = 0.0f;
};

struct GlyphEntry
{
    uint32_t codepoint = 0;
    Glyph glyph;
};

struct KerningPair
{
    uint32_t left = 0;
    uint32_t right = 0;
    float advance = 0.0f;
};

static_assert(std::is_trivially_copyable_v<GlyphEntry> && sizeof(GlyphEntry) == 40);
static_assert(std::is_trivially_copyable_v<KerningPair> && sizeof(KerningPair) == 12);

inline constexpr uint32_t GLYPH_ATLAS_MAX_GLYPHS = 4096;
inline constexpr float GLYPH_ATLAS_MAX_PIXEL_HEIGHT = 1024.0f;

// Throws Error for a non-positive or oversized pixel height, an invalid range or more than GLYPH_ATLAS_MAX_GLYPHS codepoints.
void validate_glyph_atlas_desc(const GlyphAtlasDesc& desc);

// The sorted, deduplicated codepoints the ranges cover; validates like validate_glyph_atlas_desc.
[[nodiscard]] std::vector<uint32_t> glyph_atlas_codepoints(const GlyphAtlasDesc& desc);

// One font at one pixel height, baked to an R8 coverage sheet and self-contained: glyph metrics and kerning are copied out of the font.
// Pure CPU data, so it can be cached to disk and uploaded by whichever renderer wants it.
class GlyphAtlasData
{
public:
    GlyphAtlasData(GlyphAtlasDesc desc, uint32_t side, std::vector<uint8_t> pixels, std::vector<GlyphEntry> glyphs, std::vector<KerningPair> kerning, float ascent, float line_height);

    // Falls back from the codepoint to U+FFFD, then '?', then an empty glyph with no advance.
    [[nodiscard]] const Glyph& glyph(uint32_t codepoint) const;
    [[nodiscard]] float kerning(uint32_t left, uint32_t right) const;

    [[nodiscard]] const GlyphAtlasDesc& desc() const { return m_desc; }
    // The sheet is side x side R8 pixels.
    [[nodiscard]] uint32_t side() const { return m_side; }
    [[nodiscard]] const std::vector<uint8_t>& pixels() const { return m_pixels; }
    [[nodiscard]] const std::vector<GlyphEntry>& glyphs() const { return m_glyphs; }
    [[nodiscard]] const std::vector<KerningPair>& kerning_pairs() const { return m_kerning; }
    [[nodiscard]] float ascent() const { return m_ascent; }
    [[nodiscard]] float line_height() const { return m_line_height; }
    [[nodiscard]] float px_range() const { return 0.0f; }

private:
    [[nodiscard]] const Glyph* find(uint32_t codepoint) const;

    GlyphAtlasDesc m_desc;
    std::vector<uint8_t> m_pixels;
    std::vector<GlyphEntry> m_glyphs;
    std::vector<KerningPair> m_kerning;
    float m_ascent;
    float m_line_height;
    uint32_t m_side;
};

} // namespace oryx
