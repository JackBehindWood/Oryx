#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "FakeFontSource.h"

using namespace oryx;

static_assert(!std::is_copy_constructible_v<Font> && std::is_move_constructible_v<Font>);

TEST_CASE("Font: atlases are created lazily per whole pixel height")
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    CHECK(font.atlas_count() == 0);
    CHECK(font.ready());

    GlyphAtlas& small = font.atlas(16.0f);
    CHECK(small.data().desc().pixel_height == 16.0f);
    CHECK(&font.atlas(16.4f) == &small);
    CHECK(font.atlas_count() == 1);
    CHECK(source->bakes == 1);
    CHECK(font.atlas(32.0f).data().desc().pixel_height == 32.0f);
    CHECK(font.atlas_count() == 2);
    CHECK_THROWS_AS((void)font.atlas(0.0f), Error);
    CHECK_THROWS_AS((void)font.atlas(GLYPH_ATLAS_MAX_PIXEL_HEIGHT * 2.0f), Error);
}

TEST_CASE("Font: the description's ranges and padding reach the source")
{
    UniquePtr<test::FakeFontSource> owned = create_unique<test::FakeFontSource>();
    GlyphAtlasDesc desc;
    desc.padding = 3;
    desc.ranges = { { 'A', 'Z' } };
    Font font = Font::create(std::move(owned), desc);
    CHECK(font.atlas(20.0f).data().desc().padding == 3);
    CHECK(font.atlas(20.0f).data().desc().ranges.size() == 1);
}

TEST_CASE("Font: measure and metrics come from the baked atlas")
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    CHECK(font.line_height(16.0f) == 20.0f);
    CHECK(font.ascent(16.0f) == 12.0f);
    const TextExtent extent = font.measure("AA\nA", 16.0f);
    CHECK(extent.width == doctest::Approx(20.0f));
    CHECK(extent.height == doctest::Approx(40.0f));
    CHECK(font.measure("AV", 16.0f).width == doctest::Approx(10.0f - 2.0f + 10.0f));
    CHECK(font.measure("AA", 16.0f, 3.0f).width == doctest::Approx(60.0f));
}

TEST_CASE("Font: the least recently used atlas is dropped beyond the cap")
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    for (uint32_t i = 0; i < FONT_MAX_ATLASES; ++i)
    {
        (void)font.atlas(static_cast<float>(10 + i));
    }
    CHECK(font.atlas_count() == FONT_MAX_ATLASES);
    (void)font.atlas(10.0f);
    (void)font.atlas(99.0f);
    CHECK(font.atlas_count() == FONT_MAX_ATLASES);
    const uint32_t bakes = source->bakes;
    (void)font.atlas(10.0f);
    CHECK(source->bakes == bakes);
    (void)font.atlas(11.0f);
    CHECK(source->bakes == bakes + 1);
}

TEST_CASE("Font: not ready until the source is, bakes again when the revision changes")
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    source->is_ready = false;

    CHECK_FALSE(font.ready());
    CHECK_THROWS_AS((void)font.atlas(16.0f), Error);
    CHECK(font.measure("A", 16.0f).width == 0.0f);
    CHECK(font.line_height(16.0f) == 0.0f);

    source->is_ready = true;
    CHECK(font.ready());
    (void)font.atlas(16.0f);
    CHECK(source->bakes == 1);

    source->current_revision = 2;
    CHECK(font.revision() == 2);
    (void)font.atlas(16.0f);
    CHECK(source->bakes == 2);
    CHECK(font.atlas_count() == 1);
    (void)font.atlas(16.0f);
    CHECK(source->bakes == 2);
}

TEST_CASE("Font: release_atlases rebuilds on demand, a missing source or bad description throws")
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    (void)font.atlas(16.0f);
    font.release_atlases();
    CHECK(font.atlas_count() == 0);
    (void)font.atlas(16.0f);
    CHECK(source->bakes == 2);

    CHECK_THROWS_AS(Font::create(nullptr), Error);
    GlyphAtlasDesc bad;
    bad.ranges = { { 9, 3 } };
    CHECK_THROWS_AS(Font::create(create_unique<test::FakeFontSource>(), bad), Error);
}

TEST_CASE("GlyphAtlas: the texture is uploaded once, as R8, and holds no device")
{
    NullRHI rhi;
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    GlyphAtlas& atlas = font.atlas(16.0f);
    const Texture2D& texture = atlas.texture(rhi);
    CHECK(&atlas.texture(rhi) == &texture);
    CHECK(texture.texture()->format() == RHIFormat::R8Unorm);
    CHECK(texture.width() == atlas.data().side());

    std::vector<uint8_t> pixels(atlas.data().pixels().size());
    rhi.read_texture(&texture.rhi(), pixels.data(), static_cast<uint32_t>(pixels.size()));
    CHECK(pixels == atlas.data().pixels());
}
