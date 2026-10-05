#include "doctest.h"

#include "Oryx.h"
#include "AssetTestSupport.h"

using namespace oryx;

namespace
{

std::filesystem::path font_path()
{
    return std::filesystem::path(OX_TEST_DATA_DIR) / "fonts" / "PressStart2P-Regular.ttf";
}

struct BakeFixture
{
    test::AssetTempDir dir;
    AssetManager assets{ settings() };
    AssetHandle<FontAsset> font = test::load_now<FontAsset>(assets, font_path());

    AssetSettings settings() const
    {
        AssetSettings result;
        result.cache_dir = dir.path() / "cache";
        return result;
    }

    static GlyphAtlasDesc desc(float pixel_height = 16.0f)
    {
        GlyphAtlasDesc result;
        result.pixel_height = pixel_height;
        return result;
    }

    GlyphAtlasBake bake(const GlyphAtlasDesc& atlas_desc) { return bake_glyph_atlas(assets.get(font), atlas_desc, assets.cache()); }

    void truncate_cache_entries() const
    {
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(dir.path() / "cache"))
        {
            if (entry.is_regular_file() && entry.path().string().find("glyph-atlas") != std::string::npos)
            {
                std::filesystem::resize_file(entry.path(), entry.file_size() / 2);
            }
        }
    }
};

} // namespace

TEST_CASE("bake_glyph_atlas: glyph metrics match the rasteriser")
{
    BakeFixture f;
    const GlyphAtlasBake bake = f.bake(f.desc());
    const GlyphAtlasData& atlas = bake.data;
    const FontAsset& font = f.assets.get(f.font);
    const GlyphBitmap bitmap = font.rasterise('A', 16.0f);
    const Glyph& glyph = atlas.glyph('A');
    CHECK(glyph.size[0] == doctest::Approx(static_cast<float>(bitmap.width)));
    CHECK(glyph.size[1] == doctest::Approx(static_cast<float>(bitmap.height)));
    CHECK(glyph.advance == doctest::Approx(bitmap.advance));
    CHECK(glyph.bearing[0] == doctest::Approx(static_cast<float>(bitmap.x_offset)));
    CHECK(glyph.bearing[1] == doctest::Approx(-static_cast<float>(bitmap.y_offset + bitmap.height)));
    const FontMetrics metrics = font.metrics(16.0f);
    CHECK(atlas.ascent() == doctest::Approx(metrics.ascent));
    CHECK(atlas.line_height() == doctest::Approx(metrics.ascent - metrics.descent + metrics.line_gap));
    CHECK(atlas.pixels().size() == static_cast<size_t>(atlas.side()) * atlas.side());
    CHECK(bake.rasterised > 50);
}

TEST_CASE("bake_glyph_atlas: uv rectangles are in range, hold the glyph pixels and never overlap")
{
    BakeFixture f;
    const GlyphAtlasData atlas = f.bake(f.desc()).data;
    const float side = static_cast<float>(atlas.side());

    struct Rect
    {
        float x0, y0, x1, y1;
    };
    std::vector<Rect> rects;
    for (const GlyphEntry& entry : atlas.glyphs())
    {
        const Glyph& glyph = entry.glyph;
        if (glyph.size[0] <= 0.0f)
        {
            continue;
        }
        CHECK(glyph.uv_min[0] >= 0.0f);
        CHECK(glyph.uv_min[1] >= 0.0f);
        CHECK(glyph.uv_max[0] <= 1.0f);
        CHECK(glyph.uv_max[1] <= 1.0f);
        rects.push_back({ glyph.uv_min[0] * side, glyph.uv_min[1] * side, glyph.uv_max[0] * side, glyph.uv_max[1] * side });
    }
    REQUIRE(rects.size() > 50);
    for (size_t i = 0; i < rects.size(); ++i)
    {
        for (size_t j = i + 1; j < rects.size(); ++j)
        {
            const bool apart = rects[i].x1 + 1.0f <= rects[j].x0 || rects[j].x1 + 1.0f <= rects[i].x0 || rects[i].y1 + 1.0f <= rects[j].y0 || rects[j].y1 + 1.0f <= rects[i].y0;
            CHECK(apart);
        }
    }

    const GlyphBitmap bitmap = f.assets.get(f.font).rasterise('A', 16.0f);
    const Glyph& a = atlas.glyph('A');
    const uint32_t x = static_cast<uint32_t>(a.uv_min[0] * side + 0.5f);
    const uint32_t y = static_cast<uint32_t>(a.uv_min[1] * side + 0.5f);
    for (int32_t row = 0; row < bitmap.height; ++row)
    {
        CHECK(std::memcmp(&atlas.pixels()[static_cast<size_t>(y + row) * atlas.side() + x], &bitmap.coverage[static_cast<size_t>(row * bitmap.width)], static_cast<size_t>(bitmap.width)) == 0);
    }
}

TEST_CASE("GlyphAtlasData: missing glyphs fall back to the replacement, then '?', then nothing")
{
    BakeFixture f;
    const GlyphAtlasData atlas = f.bake(f.desc()).data;
    const Glyph& question = atlas.glyph('?');
    REQUIRE(question.size[0] > 0.0f);
    CHECK(&atlas.glyph(0x4E2D) == &question);
    CHECK(&atlas.glyph(UTF8_REPLACEMENT) == &question);

    GlyphAtlasDesc digits = f.desc();
    digits.ranges = { { '0', '9' } };
    const GlyphAtlasData sparse = f.bake(digits).data;
    const Glyph& none = sparse.glyph('A');
    CHECK(none.size[0] == 0.0f);
    CHECK(none.advance == 0.0f);
}

TEST_CASE("bake_glyph_atlas: bakes the Latin-1 supplement by default")
{
    BakeFixture f;
    const GlyphAtlasData atlas = f.bake(f.desc()).data;
    const FontAsset& font = f.assets.get(f.font);
    uint32_t checked = 0;
    for (uint32_t codepoint = 0xA1; codepoint <= 0xFF; ++codepoint)
    {
        if (font.has_glyph(codepoint))
        {
            CHECK(atlas.glyph(codepoint).advance == doctest::Approx(font.rasterise(codepoint, 16.0f).advance));
            ++checked;
        }
    }
    CHECK(checked > 0);
}

TEST_CASE("bake_glyph_atlas: kerning is baked from the font")
{
    BakeFixture f;
    const GlyphAtlasData atlas = f.bake(f.desc(24.0f)).data;
    const FontAsset& font = f.assets.get(f.font);
    const char pairs[] = "AVTAY.LTWA";
    for (size_t i = 0; i + 1 < sizeof(pairs) - 1; ++i)
    {
        const uint32_t left = static_cast<uint32_t>(pairs[i]);
        const uint32_t right = static_cast<uint32_t>(pairs[i + 1]);
        CHECK(atlas.kerning(left, right) == doctest::Approx(font.kerning(left, right, 24.0f)));
    }
    for (const KerningPair& pair : atlas.kerning_pairs())
    {
        CHECK(atlas.kerning(pair.left, pair.right) == pair.advance);
    }
}

TEST_CASE("layout_text: measures lines, advances and kerning, and skips controls")
{
    BakeFixture f;
    const GlyphAtlasData atlas = f.bake(f.desc()).data;
    const float advance = atlas.glyph('A').advance;

    const TextExtent empty = layout_text(atlas, "", 1.0f, [](const Glyph&, const Vec2f&) {});
    CHECK(empty.width == 0.0f);
    CHECK(empty.height == 0.0f);

    uint32_t count = 0;
    const TextExtent one = layout_text(atlas, "A", 2.0f, [&count](const Glyph&, const Vec2f&) { ++count; });
    CHECK(count == 1);
    CHECK(one.width == doctest::Approx(advance * 2.0f));
    CHECK(one.height == doctest::Approx(atlas.line_height() * 2.0f));

    std::vector<Vec2f> pens;
    const TextExtent two = layout_text(atlas, "A\r\nAA", 1.0f, [&pens](const Glyph&, const Vec2f& pen) { pens.push_back(pen); });
    REQUIRE(pens.size() == 3);
    CHECK(pens[0][1] == 0.0f);
    CHECK(pens[1][0] == 0.0f);
    CHECK(pens[1][1] == doctest::Approx(-atlas.line_height()));
    CHECK(pens[2][0] == doctest::Approx(advance + atlas.kerning('A', 'A')));
    CHECK(two.height == doctest::Approx(atlas.line_height() * 2.0f));
    CHECK(two.width == doctest::Approx(2.0f * advance + atlas.kerning('A', 'A')));

    std::string accented;
    encode_utf8(0xE9, accented);
    count = 0;
    (void)layout_text(atlas, accented + accented, 1.0f, [&count](const Glyph&, const Vec2f&) { ++count; });
    CHECK(count == 2);
}

TEST_CASE("bake_glyph_atlas: the cache gives identical data without rasterising")
{
    BakeFixture f;
    const GlyphAtlasBake first = f.bake(f.desc());
    CHECK(first.rasterised > 0);
    CHECK_FALSE(first.cache_hit);

    const GlyphAtlasBake second = f.bake(f.desc());
    CHECK(second.rasterised == 0);
    CHECK(second.cache_hit);
    CHECK(second.data.pixels() == first.data.pixels());
    CHECK(second.data.glyphs().size() == first.data.glyphs().size());
    CHECK(second.data.kerning_pairs().size() == first.data.kerning_pairs().size());
    CHECK(second.data.line_height() == first.data.line_height());
    CHECK(second.data.glyph('A').uv_min[0] == first.data.glyph('A').uv_min[0]);
}

TEST_CASE("bake_glyph_atlas: pixel height, ranges, padding and font bytes change the cache key")
{
    BakeFixture f;
    (void)f.bake(f.desc(16.0f));

    CHECK_FALSE(f.bake(f.desc(20.0f)).cache_hit);

    GlyphAtlasDesc ranges = f.desc(16.0f);
    ranges.ranges = { { 0x20, 0x7E } };
    CHECK_FALSE(f.bake(ranges).cache_hit);

    GlyphAtlasDesc padded = f.desc(16.0f);
    padded.padding = 2;
    CHECK_FALSE(f.bake(padded).cache_hit);

    std::vector<uint8_t> bytes = read_binary_file(font_path());
    bytes.back() ^= 0xFF;
    test::write_bytes(f.dir.path() / "changed.ttf", bytes);
    AssetHandle<FontAsset> changed = test::load_now<FontAsset>(f.assets, f.dir.path() / "changed.ttf");
    const GlyphAtlasBake other = bake_glyph_atlas(f.assets.get(changed), f.desc(16.0f), f.assets.cache());
    CHECK_FALSE(other.cache_hit);
    CHECK(other.rasterised > 0);
}

TEST_CASE("bake_glyph_atlas: a truncated cache entry falls back to rasterising")
{
    BakeFixture f;
    const GlyphAtlasBake first = f.bake(f.desc());
    f.truncate_cache_entries();
    const GlyphAtlasBake second = f.bake(f.desc());
    CHECK_FALSE(second.cache_hit);
    CHECK(second.rasterised > 0);
    CHECK(second.data.pixels() == first.data.pixels());
}

TEST_CASE("bake_glyph_atlas: rejects invalid descriptions")
{
    BakeFixture f;
    GlyphAtlasDesc desc;
    desc.pixel_height = 0.0f;
    CHECK_THROWS_AS((void)f.bake(desc), Error);
    desc.pixel_height = GLYPH_ATLAS_MAX_PIXEL_HEIGHT + 1.0f;
    CHECK_THROWS_AS((void)f.bake(desc), Error);
    desc.pixel_height = 16.0f;
    desc.ranges = { { 10, 5 } };
    CHECK_THROWS_AS((void)f.bake(desc), Error);
    desc.ranges = { { 0, 0x110000 } };
    CHECK_THROWS_AS((void)f.bake(desc), Error);
    desc.ranges = { { 0x4E00, 0x4E00 + GLYPH_ATLAS_MAX_GLYPHS } };
    CHECK_THROWS_AS((void)f.bake(desc), Error);
}

TEST_CASE("GlyphAtlasData: rejects pixels that do not match the side")
{
    CHECK_THROWS_AS(GlyphAtlasData(GlyphAtlasDesc{}, 8, std::vector<uint8_t>(10), {}, {}, 0.0f, 0.0f), Error);
}

TEST_CASE("AssetFontSource: not ready while the font loads, revision follows a reload")
{
    test::AssetTempDir dir;
    AssetManager assets{ test::uncached_settings() };
    AssetHandle<FontAsset> font = assets.load<FontAsset>(font_path());
    AssetFontSource source(assets, font);

    CHECK_FALSE(source.ready());
    CHECK_THROWS_AS((void)source.bake(GlyphAtlasDesc{}), Error);

    assets.wait(font);
    REQUIRE(source.ready());
    const uint32_t revision = source.revision();
    const GlyphAtlasData data = source.bake(GlyphAtlasDesc{});
    CHECK(data.glyph('A').size[0] > 0.0f);
    CHECK(source.stats().bakes == 1);
    CHECK(source.stats().rasterised > 0);
    CHECK(source.stats().cache_hits == 0);

    assets.reload(font);
    assets.wait(font);
    CHECK(source.revision() == revision + 1);

    UniquePtr<IFontSource> erased = make_font_source(assets, font);
    CHECK(erased->ready());
}
