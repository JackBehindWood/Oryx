#include "doctest.h"

#include "AssetTestSupport.h"

namespace
{

std::filesystem::path font_path()
{
    return std::filesystem::path(OX_TEST_DATA_DIR) / "fonts" / "PressStart2P-Regular.ttf";
}

} // namespace

TEST_CASE("FontAsset::from_bytes rejects garbage")
{
    CHECK_THROWS_AS(oryx::FontAsset::from_bytes({ 1, 2, 3, 4 }), oryx::Error);
    CHECK_THROWS_AS(oryx::FontAsset::from_bytes({}), oryx::Error);
}

TEST_CASE("FontAsset reports glyph coverage")
{
    oryx::FontAsset font = oryx::FontAsset::from_bytes(oryx::read_binary_file(font_path()));
    CHECK(font.has_glyph('A'));
    CHECK_FALSE(font.has_glyph(0x10FFFF));
}

TEST_CASE("FontAsset::rasterise produces an A8 bitmap")
{
    oryx::FontAsset font = oryx::FontAsset::from_bytes(oryx::read_binary_file(font_path()));
    oryx::GlyphBitmap small = font.rasterise('A', 16.0f);
    oryx::GlyphBitmap large = font.rasterise('A', 48.0f);

    REQUIRE(small.width > 0);
    REQUIRE(small.height > 0);
    CHECK(small.coverage.size() == static_cast<size_t>(small.width * small.height));
    CHECK(std::any_of(small.coverage.begin(), small.coverage.end(), [](uint8_t value) { return value != 0; }));
    CHECK(small.advance > 0.0f);
    CHECK(large.width > small.width);
    CHECK(large.height > small.height);
}

TEST_CASE("FontAsset::rasterise returns an empty bitmap with an advance for space")
{
    oryx::FontAsset font = oryx::FontAsset::from_bytes(oryx::read_binary_file(font_path()));
    oryx::GlyphBitmap space = font.rasterise(' ', 32.0f);
    CHECK(space.coverage.empty());
    CHECK(space.width == 0);
    CHECK(space.advance > 0.0f);
}

TEST_CASE("FontAsset::metrics scale with pixel height")
{
    oryx::FontAsset font = oryx::FontAsset::from_bytes(oryx::read_binary_file(font_path()));
    oryx::FontMetrics small = font.metrics(16.0f);
    oryx::FontMetrics large = font.metrics(32.0f);
    CHECK(small.ascent > 0.0f);
    CHECK(small.descent <= 0.0f);
    CHECK(large.ascent > small.ascent);
}

TEST_CASE("AssetManager loads fonts from disk")
{
    oryx::AssetManager manager(oryx::test::uncached_settings());
    oryx::AssetHandle<oryx::FontAsset> font = oryx::test::load_now<oryx::FontAsset>(manager, font_path());
    REQUIRE(manager.state(font) == oryx::AssetState::Ready);
    CHECK(manager.get(font).has_glyph('A'));

    oryx::AssetHandle<oryx::FontAsset> missing = oryx::test::load_now<oryx::FontAsset>(manager, "no/such/font.ttf");
    CHECK(manager.state(missing) == oryx::AssetState::Failed);
}

TEST_CASE("FontAsset::kerning scales linearly and is zero for an unkerned pair")
{
    oryx::FontAsset font = oryx::FontAsset::from_bytes(oryx::read_binary_file(font_path()));
    CHECK(font.kerning('A', 'A', 16.0f) == doctest::Approx(0.0f));
    CHECK(font.kerning('A', 'V', 32.0f) == doctest::Approx(font.kerning('A', 'V', 16.0f) * 2.0f));
}

TEST_CASE("FontAsset::content_hash is stable and depends on the bytes")
{
    std::vector<uint8_t> bytes = oryx::read_binary_file(font_path());
    oryx::FontAsset first = oryx::FontAsset::from_bytes(bytes);
    oryx::FontAsset second = oryx::FontAsset::from_bytes(bytes);
    CHECK(first.content_hash() == second.content_hash());
    bytes.back() ^= 0xFF;
    oryx::FontAsset changed = oryx::FontAsset::from_bytes(bytes);
    CHECK(changed.content_hash() != first.content_hash());
}
