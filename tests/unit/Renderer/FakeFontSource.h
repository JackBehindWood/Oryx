#pragma once

#include "Oryx.h"

namespace oryx::test
{

// A font source that needs no asset system: a 64x64 sheet with fixed glyphs scaled by pixel_height / 16.
// 'A' 'B' 'C' 'D' 'E' 'V' '?' and U+00E9 are 8x8 with advance 10, bearing (1, 0); space has no bitmap and advance 6; kerning('A', 'V') is -2; ascent 12, line height 20.
class FakeFontSource final : public IFontSource
{
public:
    bool is_ready = true;
    uint32_t current_revision = 1;
    uint32_t bakes = 0;

    [[nodiscard]] bool ready() const override { return is_ready; }
    [[nodiscard]] uint32_t revision() const override { return current_revision; }

    [[nodiscard]] GlyphAtlasData bake(const GlyphAtlasDesc& desc) override
    {
        if (!is_ready)
        {
            throw Error("fake font is not ready");
        }
        ++bakes;
        const float s = desc.pixel_height / 16.0f;
        const uint32_t side = 64;
        std::vector<uint8_t> pixels(side * side, 255);
        std::vector<GlyphEntry> glyphs;
        glyphs.push_back({ ' ', Glyph{ {}, {}, {}, {}, 6.0f * s } });
        const uint32_t drawable[] = { '?', 'A', 'B', 'C', 'D', 'E', 'V', 0xE9 };
        for (uint32_t i = 0; i < sizeof(drawable) / sizeof(drawable[0]); ++i)
        {
            const float x = static_cast<float>(i * 8) / side;
            glyphs.push_back({ drawable[i], Glyph{ { x, 0.0f }, { x + 8.0f / side, 8.0f / side }, { 8.0f * s, 8.0f * s }, { 1.0f * s, 0.0f }, 10.0f * s } });
        }
        std::sort(glyphs.begin(), glyphs.end(), [](const GlyphEntry& a, const GlyphEntry& b) { return a.codepoint < b.codepoint; });
        std::vector<KerningPair> kerning = { { 'A', 'V', -2.0f * s } };
        return GlyphAtlasData(desc, side, std::move(pixels), std::move(glyphs), std::move(kerning), 12.0f * s, 20.0f * s);
    }
};

inline Font make_fake_font(FakeFontSource*& source)
{
    UniquePtr<FakeFontSource> owned = create_unique<FakeFontSource>();
    source = owned.get();
    return Font::create(std::move(owned));
}

} // namespace oryx::test
