#include "doctest.h"

#include "ShaderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

const char* SLANG_SOURCE = R"(
struct Frame { float4x4 view_projection; float4 tint; };
ConstantBuffer<Frame> frame;
Texture2D<float4> textures[4];
SamplerState smp;

struct VIn { float3 position : POSITION; float2 uv : TEXCOORD0; };
struct VOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
VOut vs_main(VIn v)
{
    VOut o;
    o.position = mul(frame.view_projection, float4(v.position, 1.0));
    o.uv = v.uv;
    return o;
}

[shader("fragment")]
float4 ps_main(VOut v) : SV_Target
{
    return frame.tint * textures[1].Sample(smp, v.uv);
}

[shader("compute")]
[numthreads(8, 4, 1)]
void cs_main(uint3 id : SV_DispatchThreadID) {}
)";

ShaderCompilerInput slang_input(ShaderStage stage, const std::string& entry, const std::string& text = SLANG_SOURCE)
{
    ShaderCompilerInput input = make_input(stage, entry, text);
    input.source = { "test.slang", ShaderLanguage::Slang, text };
    return input;
}

bool slangc_available()
{
    try
    {
        (void)SlangCompiler().compile(slang_input(ShaderStage::Vertex, "vs_main"));
        return true;
    }
    catch (const Error&)
    {
        return false;
    }
}

std::string slang_error(const ShaderCompilerInput& input)
{
    try
    {
        (void)SlangCompiler().compile(input);
    }
    catch (const Error& error)
    {
        return error.what();
    }
    return {};
}

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

const ShaderStageVariable* find_variable(const std::vector<ShaderStageVariable>& variables, const std::string& name)
{
    for (const ShaderStageVariable& variable : variables)
    {
        if (variable.name == name)
        {
            return &variable;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("Slang compiler: dependencies scans includes without touching the tool")
{
    const ShaderSource source{ "x.slang", ShaderLanguage::Slang, "#include \"Oryx/Common.slang\"\n// #include \"commented\"\n" };
    const std::vector<std::string> dependencies = SlangCompiler().dependencies(source);
    CHECK(std::find(dependencies.begin(), dependencies.end(), "Oryx/Common.slang") != dependencies.end());
    CHECK(SlangCompiler().id() == std::string("oryx-slang"));
    CHECK(shader_language_for_path("/Oryx/Builtin/Quad.slang") == ShaderLanguage::Slang);
    CHECK(&shader_compiler_for(ShaderLanguage::Slang) != &shader_compiler_for(ShaderLanguage::MSL));
}

TEST_CASE("Slang compiler: a missing tool is an actionable error")
{
    setenv("OX_SLANGC", "/nonexistent/slangc", 1);
    const std::string message = slang_error(slang_input(ShaderStage::Vertex, "vs_main"));
    unsetenv("OX_SLANGC");
    CHECK(contains(message, "cannot run slangc"));
}

TEST_CASE("Slang compiler: OX_SLANGC beats shaders.slangc, which beats the built-in default")
{
    unsetenv("OX_SLANGC");
    CHECK(slangc_path({ "/opt/slangc" }) == std::filesystem::path("/opt/slangc"));
    setenv("OX_SLANGC", "/env/slangc", 1);
    CHECK(slangc_path({ "/opt/slangc" }) == std::filesystem::path("/env/slangc"));
    unsetenv("OX_SLANGC");
    CHECK_FALSE(slangc_path().empty());
}

TEST_CASE("Slang compiler: a bad shaders.slangc gives the actionable error and a cache hit ignores it")
{
    unsetenv("OX_SLANGC");
    const ShaderCompilerInput input = slang_input(ShaderStage::Vertex, "vs_main");
    ShaderCache cache;
    cache.set_slang_options({ "/nonexistent/slangc" });
    try
    {
        (void)cache.get_or_compile(input);
        FAIL("expected the compile to fail");
    }
    catch (const Error& error)
    {
        CHECK(contains(error.what(), "cannot run slangc at '/nonexistent/slangc'"));
    }
    CHECK(contains(SlangCompiler({ "/nonexistent/slangc" }).id(), "slang"));
    CHECK(SlangCompiler({ "/a" }).version() == SlangCompiler().version());
}

TEST_CASE("Slang compiler: a store hit never needs slangc")
{
    setenv("OX_SLANGC", "/nonexistent/slangc", 1);
    ShaderCompilerOutput stored;
    stored.binary = { 'x' };
    stored.language = ShaderLanguage::Slang;
    stored.compiler_id = "oryx-slang";
    stored.compiler_version = SlangCompiler().version();
    stored.reflection.entry_point = "vs_main";
    const ShaderCompilerInput input = slang_input(ShaderStage::Vertex, "vs_main");
    class MemoryStore final : public IShaderBinaryStore
    {
    public:
        bool read(ShaderStoreKind kind, uint64_t key, std::vector<uint8_t>& payload) const override
        {
            const auto it = entries.find({ kind, key });
            if (it == entries.end()) return false;
            payload = it->second;
            return true;
        }
        void write(ShaderStoreKind kind, uint64_t key, const uint8_t* bytes, size_t size) const override { entries[{ kind, key }] = std::vector<uint8_t>(bytes, bytes + size); }
        mutable std::map<std::pair<ShaderStoreKind, uint64_t>, std::vector<uint8_t>> entries;
    } store;
    write_shader_output(store, hash_shader_input(input, SlangCompiler()), stored);
    ShaderCache cache;
    cache.set_store(&store);
    CHECK(cache.get_or_compile(input).binary == stored.binary);
    CHECK(cache.stats().store_hits == 1);
    unsetenv("OX_SLANGC");
}

TEST_CASE("Slang compiler: compiles stages and reflects them")
{
    if (!slangc_available())
    {
        MESSAGE("slangc is not available; run `forge deps sync`");
        return;
    }

    const SlangCompiler compiler;
    const ShaderCompilerOutput vertex = compiler.compile(slang_input(ShaderStage::Vertex, "vs_main"));
    CHECK(vertex.format == ShaderBinaryFormat::MslSource);
    const std::string msl(vertex.binary.begin(), vertex.binary.end());
    CHECK(contains(msl, "vs_main"));
    CHECK(contains(msl, "[[buffer(0)]]"));

    const ShaderReflection& vr = vertex.reflection;
    CHECK(vr.entry_point == "vs_main");
    CHECK(vr.stage == ShaderStage::Vertex);
    const ShaderStageVariable* position = find_variable(vr.inputs, "position");
    const ShaderStageVariable* uv = find_variable(vr.inputs, "uv");
    REQUIRE(position != nullptr);
    REQUIRE(uv != nullptr);
    CHECK(position->location == 0);
    CHECK(position->type == ShaderDataType{ ShaderScalar::Float, 3, 1 });
    CHECK(uv->location == 1);
    CHECK(uv->type == ShaderDataType{ ShaderScalar::Float, 2, 1 });
    const ShaderStageVariable* clip = find_variable(vr.outputs, "position");
    REQUIRE(clip != nullptr);
    CHECK(clip->builtin);
    REQUIRE(vr.parameters.size() == 1);
    const ShaderBinding& frame = vr.parameters[0];
    CHECK(frame.name == "frame");
    CHECK(frame.kind == ShaderBindingKind::Constants);
    CHECK(frame.slot == 0);
    CHECK(frame.size == 80);
    REQUIRE(frame.members.size() == 2);
    CHECK(frame.members[0].name == "view_projection");
    CHECK(frame.members[0].size == 64);
    CHECK(frame.members[1].offset == 64);

    const ShaderCompilerOutput pixel = compiler.compile(slang_input(ShaderStage::Pixel, "ps_main"));
    const ShaderReflection& pr = pixel.reflection;
    CHECK(pr.stage == ShaderStage::Pixel);
    const ShaderBinding* textures = find_binding(pr.parameters, "textures");
    const ShaderBinding* sampler = find_binding(pr.parameters, "smp");
    REQUIRE(textures != nullptr);
    REQUIRE(sampler != nullptr);
    CHECK(textures->kind == ShaderBindingKind::SampledTexture);
    CHECK(textures->array_count == 4);
    CHECK(textures->texture_dimension == ShaderTextureDimension::Tex2D);
    CHECK(textures->slot == 0);
    CHECK(sampler->kind == ShaderBindingKind::Sampler);
    CHECK(sampler->slot == 0);
    CHECK(find_binding(pr.parameters, "frame") != nullptr);

    const ShaderCompilerOutput compute = compiler.compile(slang_input(ShaderStage::Compute, "cs_main"));
    CHECK(compute.reflection.thread_group_size[0] == 8);
    CHECK(compute.reflection.thread_group_size[1] == 4);
    CHECK(compute.reflection.thread_group_size[2] == 1);
}

TEST_CASE("Slang compiler: defines and includes reach slangc")
{
    if (!slangc_available())
    {
        return;
    }
    ShaderCompilerInput input = slang_input(ShaderStage::Pixel, "ps_main", "#include \"test/Count.slang\"\nTexture2D<float4> textures[COUNT];\nSamplerState smp;\n"
                                                                            "[shader(\"fragment\")] float4 ps_main(float2 uv : TEXCOORD0) : SV_Target { return textures[0].Sample(smp, uv); }\n");
    input.includes["test/Count.slang"] = "#ifndef COUNT\n#define COUNT 2\n#endif\n";
    CHECK(SlangCompiler().compile(input).reflection.parameters[0].array_count == 2);
    input.defines.push_back({ "COUNT", "6" });
    CHECK(SlangCompiler().compile(input).reflection.parameters[0].array_count == 6);
}

TEST_CASE("Slang compiler: failures name the source and line")
{
    if (!slangc_available())
    {
        return;
    }
    const std::string syntax = slang_error(slang_input(ShaderStage::Pixel, "ps_main", "\n\nfloat4 ps_main( : SV_Target { return 0; }\n"));
    CHECK(contains(syntax, "test.slang:3:"));

    CHECK(contains(slang_error(slang_input(ShaderStage::Vertex, "missing_entry")), "missing_entry"));
    CHECK(contains(slang_error(slang_input(ShaderStage::Vertex, "ps_main")), "stage mismatch"));
    CHECK(contains(slang_error(slang_input(ShaderStage::Pixel, "ps_main", "#include \"test/Nope.slang\"\n")), "test/Nope.slang"));
}

TEST_CASE("Slang compiler: loose uniforms are rejected")
{
    if (!slangc_available())
    {
        return;
    }
    const std::string message = slang_error(slang_input(ShaderStage::Pixel, "ps_main", "uniform float4 tint;\n[shader(\"fragment\")] float4 ps_main() : SV_Target { return tint; }\n"));
    CHECK(contains(message, "ConstantBuffer"));
}

TEST_CASE("Slang compiler: built-in shaders reflect the engine's bindings")
{
    if (!slangc_available())
    {
        return;
    }
    NullRHI rhi;
    ShaderCache cache;
    ShaderLibrary library;
    library.compile_all(rhi, cache);
    const Ref<QuadVS> quad_vs = library.get<QuadVS>();
    const ShaderBinding& frame = quad_vs->binding(SHADER_FRAME_BINDING);
    CHECK(frame.kind == ShaderBindingKind::Constants);
    CHECK(frame.slot == 0);
    CHECK(frame.size == SHADER_FRAME_SIZE);
    const Ref<TextPS> text_ps = library.get<TextPS>();
    CHECK(text_ps->binding("textures").array_count == 16);
}
