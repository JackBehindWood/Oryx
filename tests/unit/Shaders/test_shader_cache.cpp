#include "doctest.h"

#include "ShaderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("hash_shader_input is deterministic and reacts to every hashed field")
{
    const ShaderCompilerInput base = make_input(ShaderStage::Vertex, "vs_main", SAMPLE_MSL, { { "A", "1" }, { "B", "2" } });
    const ShaderHash hash = hash_shader_input(base, "c", 1);
    CHECK(hash == hash_shader_input(base, "c", 1));

    ShaderCompilerInput reordered = base;
    std::swap(reordered.defines[0], reordered.defines[1]);
    CHECK(hash == hash_shader_input(reordered, "c", 1));

    ShaderCompilerInput renamed = base;
    renamed.source.name = "other.msl";
    CHECK(hash == hash_shader_input(renamed, "c", 1));

    ShaderCompilerInput changed = base;
    changed.source.text += " ";
    CHECK(hash != hash_shader_input(changed, "c", 1));
    changed = base;
    changed.entry_point = "ps_main";
    CHECK(hash != hash_shader_input(changed, "c", 1));
    changed = base;
    changed.stage = ShaderStage::Pixel;
    CHECK(hash != hash_shader_input(changed, "c", 1));
    changed = base;
    changed.defines[0].value = "3";
    CHECK(hash != hash_shader_input(changed, "c", 1));
    changed = base;
    changed.defines.pop_back();
    CHECK(hash != hash_shader_input(changed, "c", 1));
    CHECK(hash != hash_shader_input(base, "d", 1));
    CHECK(hash != hash_shader_input(base, "c", 2));
}

TEST_CASE("hash_shader_input does not alias adjacent fields")
{
    ShaderCompilerInput a = make_input(ShaderStage::Vertex, "ab", "c");
    ShaderCompilerInput b = make_input(ShaderStage::Vertex, "a", "bc");
    CHECK(hash_shader_input(a, "c", 1) != hash_shader_input(b, "c", 1));
}

TEST_CASE("hash_shader_input covers included text")
{
    register_shader_include("test/HashInclude.msl", "struct A { float a; };\n");
    const ShaderCompilerInput input = make_input(ShaderStage::Pixel, "f", "#include \"test/HashInclude.msl\"\n");
    const ShaderHash before = hash_shader_input(input, "c", 1);
    register_shader_include("test/HashInclude.msl", "struct A { float b; };\n");
    CHECK(before != hash_shader_input(input, "c", 1));
}

TEST_CASE("ShaderCache counts hits and misses and returns stable entries")
{
    ShaderCache cache;
    const ShaderCompilerInput vertex = make_input(ShaderStage::Vertex, "vs_main", SAMPLE_MSL);
    const ShaderCompilerInput pixel = make_input(ShaderStage::Pixel, "ps_main", SAMPLE_MSL);

    const ShaderCompilerOutput& first = cache.get_or_compile(vertex);
    CHECK(cache.stats().misses == 1);
    CHECK(cache.stats().hits == 0);
    CHECK(cache.find(first.hash) == &first);

    CHECK(&cache.get_or_compile(vertex) == &first);
    CHECK(cache.stats().hits == 1);

    const ShaderCompilerOutput& second = cache.get_or_compile(pixel);
    CHECK(&second != &first);
    CHECK(second.reflection.stage == ShaderStage::Pixel);
    CHECK(cache.stats().entries == 2);
    CHECK(cache.stats().misses == 2);
    CHECK(&cache.get_or_compile(vertex) == &first);

    const ShaderHash first_hash = first.hash;
    cache.clear();
    CHECK(cache.stats().entries == 0);
    CHECK(cache.stats().hits == 0);
    CHECK(cache.find(first_hash) == nullptr);
}

TEST_CASE("ShaderCache does not cache failed compiles")
{
    ShaderCache cache;
    const ShaderCompilerInput bad = make_input(ShaderStage::Vertex, "nope", SAMPLE_MSL);
    CHECK_THROWS_AS(cache.get_or_compile(bad), Error);
    CHECK_THROWS_AS(cache.get_or_compile(bad), Error);
    CHECK(cache.stats().entries == 0);
    CHECK(cache.stats().misses == 2);
}

TEST_CASE("shader_compiler_for returns the MSL compiler")
{
    IShaderCompiler& compiler = shader_compiler_for(ShaderLanguage::MSL);
    CHECK(std::string(compiler.id()) == "oryx-msl");
    CHECK(compiler.version() >= 1);
}
