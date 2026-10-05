#include "doctest.h"

#include "AssetTestSupport.h"

namespace
{

oryx::ImageAsset decode(const std::vector<uint8_t>& bytes)
{
    return oryx::ImageAsset::decode(bytes.data(), bytes.size());
}

} // namespace

TEST_CASE("ImageAsset::decode returns exact RGBA8 pixels")
{
    std::vector<uint8_t> source = { 255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0, 10, 20, 30, 40 };
    oryx::ImageAsset image = decode(oryx::test::encode_png(2, 2, 4, source));
    CHECK(image.width() == 2);
    CHECK(image.height() == 2);
    REQUIRE(image.pixel_bytes() == source.size());
    CHECK(std::equal(image.pixels(), image.pixels() + image.pixel_bytes(), source.begin()));
}

TEST_CASE("ImageAsset::decode expands RGB to opaque RGBA")
{
    std::vector<uint8_t> source = { 1, 2, 3, 4, 5, 6 };
    oryx::ImageAsset image = decode(oryx::test::encode_png(2, 1, 3, source));
    REQUIRE(image.pixel_bytes() == 8);
    std::vector<uint8_t> expected = { 1, 2, 3, 255, 4, 5, 6, 255 };
    CHECK(std::equal(image.pixels(), image.pixels() + image.pixel_bytes(), expected.begin()));
}

TEST_CASE("ImageAsset::decode throws on corrupt data")
{
    std::vector<uint8_t> garbage = { 1, 2, 3, 4, 5 };
    CHECK_THROWS_AS(decode(garbage), oryx::Error);
}

TEST_CASE("AssetManager loads images from disk")
{
    oryx::test::AssetTempDir dir;
    oryx::test::write_png(dir.path() / "tile.png", 3, 2, 4, std::vector<uint8_t>(3 * 2 * 4, 200));
    oryx::test::write_png(dir.path() / "upper.PNG", 1, 1, 4, { 1, 2, 3, 4 });

    oryx::AssetManager manager(oryx::test::uncached_settings());
    oryx::AssetHandle<oryx::ImageAsset> tile = oryx::test::load_now<oryx::ImageAsset>(manager, dir.path() / "tile.png");
    REQUIRE(manager.state(tile) == oryx::AssetState::Ready);
    CHECK(manager.get(tile).width() == 3);
    CHECK(manager.get(tile).height() == 2);

    oryx::AssetHandle<oryx::ImageAsset> upper = oryx::test::load_now<oryx::ImageAsset>(manager, dir.path() / "upper.PNG");
    CHECK(manager.state(upper) == oryx::AssetState::Ready);

    oryx::AssetHandle<oryx::ImageAsset> missing = oryx::test::load_now<oryx::ImageAsset>(manager, dir.path() / "missing.png");
    CHECK(manager.state(missing) == oryx::AssetState::Failed);
    CHECK_FALSE(manager.error(missing).empty());

    oryx::test::write_bytes(dir.path() / "broken.png", { 9, 9, 9 });
    CHECK(manager.state(oryx::test::load_now<oryx::ImageAsset>(manager, dir.path() / "broken.png")) == oryx::AssetState::Failed);
}
