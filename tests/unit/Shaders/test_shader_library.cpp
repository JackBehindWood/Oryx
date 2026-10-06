#include "doctest.h"

#include "ShaderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

constexpr const char* TEST_SOURCE = R"msl(
#include <metal_stdlib>
using namespace metal;

#ifndef N
#define N 1
#endif

fragment float4 test_ps(array<texture2d<float>, N> t [[texture(0)]])
{
    return float4(0);
}
)msl";

struct LibraryTestDimension
{
    static constexpr const char* NAME = "N";
    static constexpr uint32_t VALUES[] = { 1, 2, 3 };
};

class LibraryTestPS : public StaticShader<PixelShader, ShaderPermutationDomain<LibraryTestDimension>>
{
public:
    using StaticShader::StaticShader;
};

const EmbeddedShaderRegistrar library_test_source("/Test/Library.msl", TEST_SOURCE, std::strlen(TEST_SOURCE));

OX_REGISTER_SHADER(LibraryTestPS, "/Test/Library.msl", "test_ps", ShaderStage::Pixel)

} // namespace

TEST_CASE("Shader types self-register")
{
    CHECK(std::string(shader_type_of<LibraryTestPS>().name) == "LibraryTestPS");
    CHECK(shader_type_of<LibraryTestPS>().stage == ShaderStage::Pixel);
    CHECK(shader_type_of<LibraryTestPS>().should_compile(2));
    CHECK_FALSE(shader_type_of<LibraryTestPS>().should_compile(3));
    CHECK(registered_shader_types().size() >= 7);
}

TEST_CASE("ShaderLibrary selects permutations")
{
    NullRHI rhi;
    ShaderCache cache;
    ShaderLibrary library;
    library.compile(rhi, cache, shader_type_of<LibraryTestPS>());

    CHECK(library.size() == 3);
    for (uint32_t permutation = 0; permutation < 3; ++permutation)
    {
        const Ref<LibraryTestPS> shader = library.get<LibraryTestPS>(permutation);
        CHECK(shader->permutation() == permutation);
        CHECK(shader->binding("t").array_count == permutation + 1);
        CHECK(shader->rhi()->stage() == RHIShaderStage::Pixel);
    }
    CHECK_FALSE(library.contains<LibraryTestPS>(3));
    CHECK_THROWS_AS(library.get<LibraryTestPS>(3), Error);
    CHECK_THROWS_AS(library.get<SolidVS>(), Error);
}

TEST_CASE("ShaderLibrary compiles through the cache")
{
    NullRHI rhi;
    ShaderCache cache;
    ShaderLibrary first;
    first.compile(rhi, cache, shader_type_of<LibraryTestPS>());
    CHECK(cache.stats().misses == 3);
    CHECK(cache.stats().hits == 0);

    ShaderLibrary second;
    second.compile(rhi, cache, shader_type_of<LibraryTestPS>());
    CHECK(cache.stats().misses == 3);
    CHECK(cache.stats().hits == 3);
    CHECK(first.get<LibraryTestPS>(1)->hash() == second.get<LibraryTestPS>(1)->hash());
    CHECK(first.get<LibraryTestPS>(1).get() != second.get<LibraryTestPS>(1).get());
}

TEST_CASE("ShaderLibrary names the shader whose compile failed")
{
    NullRHI rhi;
    ShaderCache cache;
    ShaderLibrary library;
    ShaderType broken = shader_type_of<LibraryTestPS>();
    broken.source = "garbage here";
    std::string message;
    try
    {
        library.compile(rhi, cache, broken);
    }
    catch (const Error& error)
    {
        message = error.what();
    }
    CHECK(message.find("LibraryTestPS (permutation 0)") != std::string::npos);
    CHECK(library.size() == 0);
}

TEST_CASE("ShaderLibrary compile_all builds every registered type and clear empties it")
{
    NullRHI rhi;
    ShaderCache cache;
    ShaderLibrary library;
    library.compile_all(rhi, cache);
    CHECK(library.contains<SolidVS>());
    CHECK(library.contains<CirclePS>());
    CHECK(library.contains<LibraryTestPS>(2));
    library.clear();
    CHECK(library.size() == 0);
}
