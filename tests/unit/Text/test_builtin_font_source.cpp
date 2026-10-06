#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

TEST_CASE("BuiltinFontSource: always ready at revision 1")
{
    BuiltinFontSource source;
    CHECK(source.ready());
    CHECK(source.revision() == 1);
}

TEST_CASE("BuiltinFontSource: bakes Basic Latin and Latin-1 with sane metrics")
{
    BuiltinFontSource source;
    GlyphAtlasDesc desc;
    desc.pixel_height = 16.0f;
    const GlyphAtlasData atlas = source.bake(desc);

    CHECK(atlas.glyphs().size() >= 190);
    CHECK(atlas.ascent() > 10.0f);
    CHECK(atlas.line_height() > 15.0f);
    CHECK(atlas.glyph('A').advance > 8.0f);
    CHECK(atlas.glyph('A').size[0] > 0.0f);
    CHECK(atlas.glyph('i').advance < atlas.glyph('W').advance);
    CHECK(atlas.glyph(' ').size[0] == 0.0f);
    CHECK(atlas.glyph(' ').advance > 0.0f);
    CHECK(atlas.pixels().size() == static_cast<size_t>(atlas.side()) * atlas.side());
}

TEST_CASE("BuiltinFontSource: glyph rectangles are in range, inked and do not overlap")
{
    BuiltinFontSource source;
    const GlyphAtlasData atlas = source.bake({});
    const float side = static_cast<float>(atlas.side());

    for (size_t i = 0; i < atlas.glyphs().size(); ++i)
    {
        const Glyph& a = atlas.glyphs()[i].glyph;
        CHECK(a.uv_min[0] >= 0.0f);
        CHECK(a.uv_max[0] <= 1.0f);
        CHECK(a.uv_max[1] <= 1.0f);
        if (a.size[0] <= 0.0f)
        {
            continue;
        }
        for (size_t j = i + 1; j < atlas.glyphs().size(); ++j)
        {
            const Glyph& b = atlas.glyphs()[j].glyph;
            if (b.size[0] <= 0.0f)
            {
                continue;
            }
            const bool separate = a.uv_max[0] <= b.uv_min[0] || b.uv_max[0] <= a.uv_min[0] || a.uv_max[1] <= b.uv_min[1] || b.uv_max[1] <= a.uv_min[1];
            CHECK(separate);
        }
    }

    const Glyph& glyph = atlas.glyph('A');
    const uint32_t x0 = static_cast<uint32_t>(glyph.uv_min[0] * side + 0.5f);
    const uint32_t y0 = static_cast<uint32_t>(glyph.uv_min[1] * side + 0.5f);
    uint32_t inked = 0;
    for (uint32_t y = 0; y < static_cast<uint32_t>(glyph.size[1]); ++y)
    {
        for (uint32_t x = 0; x < static_cast<uint32_t>(glyph.size[0]); ++x)
        {
            inked += atlas.pixels()[static_cast<size_t>(y0 + y) * atlas.side() + x0 + x] != 0 ? 1 : 0;
        }
    }
    CHECK(inked > 0);
}

TEST_CASE("BuiltinFontSource: unmapped codepoints fall back to the replacement glyph")
{
    BuiltinFontSource source;
    const GlyphAtlasData atlas = source.bake({});
    CHECK(atlas.glyph(0x4E2D).advance == atlas.glyph(UTF8_REPLACEMENT).advance);
}

TEST_CASE("BuiltinFontSource: invalid descriptions throw")
{
    BuiltinFontSource source;
    GlyphAtlasDesc desc;
    desc.pixel_height = 0.0f;
    CHECK_THROWS_AS((void)source.bake(desc), Error);
}
