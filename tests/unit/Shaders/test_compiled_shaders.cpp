#include "doctest.h"

#include "ShaderTestSupport.h"

#include "Oryx/Shaders/ShaderBinaryStore.h"
#include "Oryx/Shaders/ShaderCook.h"
#include "Oryx/Shaders/ShaderMap.h"
#include "Oryx/Shaders/ShaderSerialisation.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

class MemoryStore final : public IShaderBinaryStore
{
public:
    bool read(ShaderStoreKind kind, uint64_t key, std::vector<uint8_t>& payload) const override
    {
        const std::map<std::pair<ShaderStoreKind, uint64_t>, std::vector<uint8_t>>::const_iterator it = entries.find({ kind, key });
        if (it == entries.end())
        {
            return false;
        }
        payload = it->second;
        return true;
    }

    void write(ShaderStoreKind kind, uint64_t key, const uint8_t* payload, size_t size) const override
    {
        entries[{ kind, key }] = std::vector<uint8_t>(payload, payload + size);
    }

    mutable std::map<std::pair<ShaderStoreKind, uint64_t>, std::vector<uint8_t>> entries;
};

class CountingCompiler final : public IShaderCompiler
{
public:
    const char* id() const override { return "fake"; }
    uint32_t version() const override { return 3; }

    std::vector<std::string> dependencies(const ShaderSource&) const override { return { "fake/dep.x" }; }

    ShaderCompilerOutput compile(const ShaderCompilerInput& input) const override
    {
        ++compiles;
        ShaderCompilerOutput output;
        output.binary.assign(input.source.text.begin(), input.source.text.end());
        output.reflection.entry_point = input.entry_point;
        output.reflection.stage = input.stage;
        return output;
    }

    mutable int compiles = 0;
};

class NeverCompiler final : public IShaderCompiler
{
public:
    const char* id() const override { return "never"; }
    uint32_t version() const override { return 1; }
    std::vector<std::string> dependencies(const ShaderSource&) const override { return {}; }
    ShaderCompilerOutput compile(const ShaderCompilerInput&) const override { throw Error("no compiler in a cooked run"); }
};

} // namespace

TEST_CASE("Compiled shader output round-trips, reflection included")
{
    const EmbeddedShaderSourceProvider sources;
    NullRHI rhi;
    size_t checked = 0;
    for (const ShaderType& type : registered_shader_types())
    {
        ShaderCompilerInput input = load_shader_input(type, sources);
        input.defines = type.defines_for(0);
        ShaderCompilerOutput output = shader_compiler_for(input.source.language).compile(input);
        output.compiler_id = "oryx-msl";
        output.compiler_version = 1;
        const std::vector<uint8_t> payload = serialise_shader_output(output);

        ShaderCompilerOutput restored;
        REQUIRE_MESSAGE(deserialise_shader_output(payload.data(), payload.size(), restored), type.name);
        CHECK(serialise_shader_output(restored) == payload);
        CHECK(restored.binary == output.binary);
        CHECK(restored.reflection.entry_point == output.reflection.entry_point);
        CHECK(restored.reflection.parameters.size() == output.reflection.parameters.size());
        CHECK(restored.reflection.inputs.size() == output.reflection.inputs.size());
        CHECK(restored.compiler_id == "oryx-msl");
        ++checked;
    }
    CHECK(checked >= 8);
}

TEST_CASE("A truncated or foreign compiled shader payload is rejected")
{
    ShaderCompilerOutput output = compile_msl(ShaderStage::Vertex, "vs_main", SAMPLE_MSL);
    const std::vector<uint8_t> payload = serialise_shader_output(output);
    for (size_t length = 0; length < payload.size(); ++length)
    {
        ShaderCompilerOutput out;
        CHECK_FALSE(deserialise_shader_output(payload.data(), length, out));
    }
    std::vector<uint8_t> foreign = payload;
    foreign[0] ^= 0xFF;
    ShaderCompilerOutput out;
    CHECK_FALSE(deserialise_shader_output(foreign.data(), foreign.size(), out));
    std::vector<uint8_t> trailing = payload;
    trailing.push_back(0);
    CHECK_FALSE(deserialise_shader_output(trailing.data(), trailing.size(), out));
}

TEST_CASE("ShaderCache reads compiled shaders from the store instead of compiling")
{
    MemoryStore store;
    CountingCompiler compiler;
    ShaderCompilerInput input = make_input(ShaderStage::Pixel, "ps_main", "// a\n", { { "A", "1" } });
    input.includes["fake/dep.x"] = "one";

    ShaderHash first_hash = 0;
    {
        ShaderCache cache;
        cache.set_store(&store);
        first_hash = cache.get_or_compile(input, compiler).hash;
        CHECK(compiler.compiles == 1);
        CHECK(cache.stats().misses == 1);
    }
    CHECK(store.entries.size() == 1);

    ShaderCache second;
    second.set_store(&store);
    const ShaderCompilerOutput& hit = second.get_or_compile(input, compiler);
    CHECK(compiler.compiles == 1);
    CHECK(second.stats().store_hits == 1);
    CHECK(hit.hash == first_hash);
    CHECK(hit.compiler_id == "fake");
    CHECK(hit.compiler_version == 3);

    input.defines = { { "A", "2" } };
    (void)second.get_or_compile(input, compiler);
    CHECK(compiler.compiles == 2);
}

TEST_CASE("Dependencies named by the compiler are hashed and invalidate the cache")
{
    CountingCompiler compiler;
    ShaderCompilerInput input = make_input(ShaderStage::Pixel, "ps_main", "// no MSL include here\n");
    input.includes["fake/dep.x"] = "one";
    const ShaderHash before = hash_shader_input(input, compiler);
    input.includes["fake/dep.x"] = "two";
    CHECK(before != hash_shader_input(input, compiler));
}

TEST_CASE("The compile target is part of the hash")
{
    CountingCompiler compiler;
    ShaderCompilerInput input = make_input(ShaderStage::Pixel, "ps_main", "// a\n");
    const ShaderHash msl = hash_shader_input(input, compiler);
    input.target = ShaderBinaryFormat::MetalLib;
    CHECK(msl != hash_shader_input(input, compiler));
}

TEST_CASE("A failed compile is not written to the store")
{
    MemoryStore store;
    ShaderCache cache;
    cache.set_store(&store);
    CHECK_THROWS_AS((void)cache.get_or_compile(make_input(ShaderStage::Vertex, "missing", SAMPLE_MSL)), Error);
    CHECK(store.entries.empty());
}

TEST_CASE("Cooked shaders load with no sources and no compiler")
{
    MemoryStore store;
    NullRHI rhi;
    {
        ShaderCache cache;
        const ShaderCookResult result = cook_shaders(cache, EmbeddedShaderSourceProvider(), store);
        CHECK(result.shaders >= 12);
        CHECK(result.map_id == shader_map_id(registered_shader_types()));
    }

    ShaderLibrary library;
    library.load_cooked(rhi, store);
    CHECK(library.contains<SolidVS>());
    CHECK(library.contains<SolidPS>());
    CHECK(library.contains<QuadPS>(1));
    CHECK(library.get<SolidVS>()->stage() == ShaderStage::Vertex);
}

TEST_CASE("Cooked loading fails clearly when the map or a binary is missing")
{
    NullRHI rhi;
    MemoryStore empty;
    ShaderLibrary library;
    CHECK_THROWS_AS(library.load_cooked(rhi, empty), Error);

    MemoryStore store;
    {
        ShaderCache cache;
        (void)cook_shaders(cache, EmbeddedShaderSourceProvider(), store);
    }
    for (auto it = store.entries.begin(); it != store.entries.end(); ++it)
    {
        if (it->first.first == ShaderStoreKind::Binary)
        {
            store.entries.erase(it);
            break;
        }
    }
    std::string message;
    try
    {
        library.load_cooked(rhi, store);
    }
    catch (const Error& error)
    {
        message = error.what();
    }
    CHECK(message.find("permutation") != std::string::npos);
    CHECK(library.size() == 0);
}

TEST_CASE("A shader map cooked for a different type set is stale")
{
    MemoryStore store;
    ShaderMap other;
    other.id = shader_map_id(registered_shader_types()) + 1;
    write_shader_map(store, other);
    NullRHI rhi;
    ShaderLibrary library;
    CHECK_THROWS_AS(library.load_cooked(rhi, store), Error);

    ShaderMap round_trip;
    other.entries.push_back({ "SolidVS", 0, 42 });
    const std::vector<uint8_t> payload = serialise_shader_map(other);
    REQUIRE(deserialise_shader_map(payload.data(), payload.size(), round_trip));
    CHECK(round_trip.find("SolidVS", 0)->hash == 42);
    CHECK(round_trip.find("SolidVS", 1) == nullptr);
    CHECK_FALSE(deserialise_shader_map(payload.data(), payload.size() - 1, round_trip));
}

TEST_CASE("A cooked run never touches a compiler")
{
    MemoryStore store;
    NullRHI rhi;
    {
        ShaderCache cache;
        (void)cook_shaders(cache, EmbeddedShaderSourceProvider(), store);
    }
    // load_cooked takes only the store and the RHI; reaching a compiler would need ShaderCache, which it never receives.
    ShaderLibrary library;
    CHECK_NOTHROW(library.load_cooked(rhi, store));
    NeverCompiler never;
    CHECK(std::string(never.id()) == "never");
}
