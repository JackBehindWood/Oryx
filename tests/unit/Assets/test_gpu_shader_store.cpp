#include "doctest.h"

#include "Oryx/Assets/GpuShaderStore.h"
#include "Oryx/Shaders/ShaderCache.h"
#include "Oryx/Shaders/ShaderCook.h"
#include "Oryx/Shaders/ShaderLibrary.h"

#include "NullRHI.h"

using namespace oryx;

namespace
{

std::filesystem::path temp_directory()
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "oryx_gpu_shader_store_test";
    std::filesystem::remove_all(path);
    return path;
}

} // namespace

TEST_CASE("GpuShaderStore keeps binaries and maps in their own typed entries")
{
    const std::filesystem::path directory = temp_directory();
    const CompiledAssetStore compiled(directory, true);
    const GpuShaderStore store(compiled);

    const std::vector<uint8_t> payload = { 1, 2, 3, 4 };
    store.write(ShaderStoreKind::Binary, 0xABCD, payload.data(), payload.size());
    store.write(ShaderStoreKind::Map, 0xABCD, payload.data(), 2);

    std::vector<uint8_t> read;
    REQUIRE(store.read(ShaderStoreKind::Binary, 0xABCD, read));
    CHECK(read == payload);
    REQUIRE(store.read(ShaderStoreKind::Map, 0xABCD, read));
    CHECK(read.size() == 2);
    CHECK_FALSE(store.read(ShaderStoreKind::Binary, 0xABCE, read));

    CHECK(compiled.entry_path(GpuShaderStore::entry_key(ShaderStoreKind::Binary, 0xABCD)).extension() == ".oxshader");
    CHECK(compiled.entry_path(GpuShaderStore::entry_key(ShaderStoreKind::Map, 0xABCD)).extension() == ".oxshadermap");
}

TEST_CASE("A cooked store on disk loads shaders in a fresh library")
{
    const std::filesystem::path directory = temp_directory();
    const CompiledAssetStore compiled(directory, true);
    const GpuShaderStore store(compiled);
    {
        ShaderCache cache;
        CHECK(cook_shaders(cache, EmbeddedShaderSourceProvider(), store).shaders > 0);
    }
    compiled.prune();

    NullRHI rhi;
    ShaderLibrary library;
    library.load_cooked(rhi, store);
    CHECK(library.size() > 0);
}
