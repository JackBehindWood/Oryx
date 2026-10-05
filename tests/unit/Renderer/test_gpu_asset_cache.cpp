#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "../Assets/AssetTestSupport.h"

using namespace oryx;

namespace
{

struct CacheFixture
{
    NullRHI rhi;
    AssetManager assets;
    GpuAssetCache cache;
    test::AssetTempDir dir;

    AssetHandle<ImageAsset> load(const char* name, int32_t width, int32_t height)
    {
        test::write_png(dir.path() / name, width, height, 4, std::vector<uint8_t>(static_cast<size_t>(width * height * 4), 90));
        return assets.load<ImageAsset>(dir.path() / name);
    }
};

} // namespace

TEST_CASE("GpuAssetCache uploads a ready image once and returns the same texture")
{
    CacheFixture f;
    AssetHandle<ImageAsset> image = f.load("a.png", 3, 2);

    const Texture2D& first = f.cache.get(f.rhi, f.assets, image);
    CHECK(first.width() == 3);
    CHECK(first.height() == 2);
    CHECK(f.cache.size() == 1);
    CHECK(f.cache.contains(image));

    const Texture2D& second = f.cache.get(f.rhi, f.assets, image);
    CHECK(&first == &second);
    CHECK(f.cache.size() == 1);

    uint8_t pixels[3 * 2 * 4] = {};
    f.rhi.read_texture(first.rhi(), pixels, sizeof(pixels));
    CHECK(pixels[0] == 90);
}

TEST_CASE("GpuAssetCache rebuilds when the slot's generation changes")
{
    CacheFixture f;
    AssetHandle<ImageAsset> old_image = f.load("a.png", 2, 2);
    RHITexturePtr old_texture = f.cache.get(f.rhi, f.assets, old_image).texture();

    f.assets.release(old_image);
    AssetHandle<ImageAsset> new_image = f.load("b.png", 4, 4);
    REQUIRE(new_image.id == old_image.id);
    REQUIRE(new_image.generation != old_image.generation);

    const Texture2D& rebuilt = f.cache.get(f.rhi, f.assets, new_image);
    CHECK(rebuilt.width() == 4);
    CHECK(rebuilt.texture() != old_texture);
    CHECK(f.cache.size() == 1);
    CHECK(f.cache.contains(new_image));
    CHECK_FALSE(f.cache.contains(old_image));
    CHECK_THROWS_AS((void)f.cache.get(f.rhi, f.assets, old_image), Error);
}

TEST_CASE("GpuAssetCache::get throws for a Failed or stale asset and caches nothing")
{
    CacheFixture f;
    AssetHandle<ImageAsset> missing = f.assets.load<ImageAsset>(f.dir.path() / "missing.png");
    REQUIRE(f.assets.state(missing) == AssetState::Failed);
    CHECK_THROWS_AS((void)f.cache.get(f.rhi, f.assets, missing), Error);
    CHECK_THROWS_AS((void)f.cache.get(f.rhi, f.assets, AssetHandle<ImageAsset>{}), Error);
    CHECK(f.cache.size() == 0);
}

TEST_CASE("GpuAssetCache release, trim and clear drop textures")
{
    CacheFixture f;
    AssetHandle<ImageAsset> a = f.load("a.png", 1, 1);
    AssetHandle<ImageAsset> b = f.load("b.png", 2, 2);
    AssetHandle<ImageAsset> c = f.load("c.png", 3, 3);
    (void)f.cache.get(f.rhi, f.assets, a);
    (void)f.cache.get(f.rhi, f.assets, b);
    (void)f.cache.get(f.rhi, f.assets, c);
    CHECK(f.cache.size() == 3);

    f.cache.release(a);
    CHECK(f.cache.size() == 2);
    CHECK_FALSE(f.cache.contains(a));
    CHECK_NOTHROW(f.cache.release(a));

    f.assets.release(b);
    f.cache.trim(f.assets);
    CHECK(f.cache.size() == 1);
    CHECK(f.cache.contains(c));

    f.cache.clear();
    CHECK(f.cache.size() == 0);
}
