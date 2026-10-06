#pragma once

#include "Oryx/Assets/Asset.h"

#include "stb_truetype.h"

namespace oryx
{

struct FontMetrics
{
    float ascent = 0.0f;
    float descent = 0.0f;
    float line_gap = 0.0f;
};

struct GlyphBitmap
{
    int32_t width = 0;
    int32_t height = 0;
    int32_t x_offset = 0;
    int32_t y_offset = 0;
    float advance = 0.0f;
    std::vector<uint8_t> coverage;
};

class FontAsset : public Asset
{
public:
    [[nodiscard]] static FontAsset from_bytes(std::vector<uint8_t> bytes);

    FontAsset(FontAsset&&) = default;
    FontAsset& operator=(FontAsset&&) = default;

    [[nodiscard]] bool has_glyph(uint32_t codepoint) const;
    [[nodiscard]] FontMetrics metrics(float pixel_height) const;
    [[nodiscard]] GlyphBitmap rasterise(uint32_t codepoint, float pixel_height) const;
    // Zero for a pair without a `kern` table entry (stb_truetype does not read GPOS).
    [[nodiscard]] float kerning(uint32_t left, uint32_t right, float pixel_height) const;
    [[nodiscard]] uint64_t content_hash() const { return m_content_hash; }
    [[nodiscard]] size_t memory_bytes() const override { return m_bytes.size(); }

private:
    explicit FontAsset(std::vector<uint8_t> bytes)
        : m_bytes(std::move(bytes))
    {
    }

    // m_info points into m_bytes' heap buffer, which a move keeps in place.
    std::vector<uint8_t> m_bytes;
    stbtt_fontinfo m_info{};
    uint64_t m_content_hash = 0;
};

} // namespace oryx
