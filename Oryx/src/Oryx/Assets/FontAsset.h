#pragma once

#include "Oryx/Core/Base.h"

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

class FontAsset
{
public:
    [[nodiscard]] static FontAsset from_bytes(std::vector<uint8_t> bytes);

    FontAsset(FontAsset&&) noexcept;
    FontAsset& operator=(FontAsset&&) noexcept;
    ~FontAsset();

    [[nodiscard]] bool has_glyph(uint32_t codepoint) const;
    [[nodiscard]] FontMetrics metrics(float pixel_height) const;
    [[nodiscard]] GlyphBitmap rasterise(uint32_t codepoint, float pixel_height) const;

private:
    struct Impl;
    explicit FontAsset(UniquePtr<Impl> impl);

    UniquePtr<Impl> m_impl;
};

} // namespace oryx
