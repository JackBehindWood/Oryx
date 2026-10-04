#include "doctest.h"

#include "ShaderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

bool contains(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

std::string fragment_with_params(const std::string& params)
{
    return "fragment float4 f(" + params + ") { return float4(0); }\n";
}

} // namespace

TEST_CASE("Shader types follow MSL size and alignment rules")
{
    ShaderDataType type;
    REQUIRE(parse_shader_type("float3", type));
    CHECK(shader_type_size(type) == 12);
    CHECK(shader_type_alignment(type) == 16);
    REQUIRE(parse_shader_type("float4x4", type));
    CHECK(shader_type_size(type) == 64);
    REQUIRE(parse_shader_type("float3x3", type));
    CHECK(shader_type_size(type) == 48);
    REQUIRE(parse_shader_type("half3", type));
    CHECK(shader_type_size(type) == 6);
    CHECK(shader_type_alignment(type) == 8);
    REQUIRE(parse_shader_type("uint2", type));
    CHECK(shader_type_size(type) == 8);
    CHECK_FALSE(parse_shader_type("float5", type));
    CHECK_FALSE(parse_shader_type("texture2d", type));
    CHECK_FALSE(parse_shader_type("int2x2", type));
    CHECK(shader_type_name({ ShaderScalar::Float, 4, 4 }) == "float4x4");
}

TEST_CASE("MslShaderCompiler reflects a vertex entry point")
{
    const ShaderCompilerOutput output = compile_msl(ShaderStage::Vertex, "vs_main", SAMPLE_MSL);
    const ShaderReflection& reflection = output.reflection;
    CHECK(reflection.entry_point == "vs_main");
    CHECK(reflection.stage == ShaderStage::Vertex);
    CHECK(output.format == ShaderBinaryFormat::MslSource);
    CHECK_FALSE(output.binary.empty());

    REQUIRE(reflection.inputs.size() == 3);
    CHECK(reflection.inputs[0].name == "position");
    CHECK(reflection.inputs[0].location == 0);
    CHECK(reflection.inputs[0].type == ShaderDataType{ ShaderScalar::Float, 3, 1 });
    CHECK(reflection.inputs[1].location == 1);
    CHECK(reflection.inputs[2].type == ShaderDataType{ ShaderScalar::Float, 2, 1 });

    REQUIRE(reflection.outputs.size() == 3);
    CHECK(reflection.outputs[0].builtin);
    CHECK_FALSE(reflection.outputs[1].builtin);
    CHECK(reflection.outputs[1].name == "colour");
    CHECK(reflection.outputs[1].location == 0);
    CHECK(reflection.outputs[2].location == 1);

    REQUIRE(reflection.parameters.size() == 1);
    const ShaderBinding& frame = reflection.parameters[0];
    CHECK(frame.name == "frame");
    CHECK(frame.kind == ShaderBindingKind::Constants);
    CHECK(frame.slot == 0);
    CHECK(frame.size == 80);
    REQUIRE(frame.members.size() == 3);
    CHECK(frame.members[0].offset == 0);
    CHECK(frame.members[0].size == 64);
    CHECK(frame.members[1].name == "tint");
    CHECK(frame.members[1].offset == 64);
    CHECK(frame.members[1].size == 12);
    CHECK(frame.members[2].name == "scale");
    CHECK(frame.members[2].offset == 76);
    CHECK(reflection.thread_group_size[0] == 0);
}

TEST_CASE("MslShaderCompiler reflects a fragment entry point")
{
    const ShaderReflection reflection = compile_msl(ShaderStage::Pixel, "ps_main", SAMPLE_MSL).reflection;
    CHECK(reflection.stage == ShaderStage::Pixel);
    REQUIRE(reflection.inputs.size() == 3);
    CHECK(reflection.inputs[0].builtin);
    CHECK(reflection.inputs[1].name == "colour");
    CHECK(reflection.inputs[2].location == 1);
    REQUIRE(reflection.outputs.size() == 1);
    CHECK(reflection.outputs[0].location == 0);

    REQUIRE(reflection.parameters.size() == 2);
    const ShaderBinding* textures = find_binding(reflection.parameters, "textures");
    REQUIRE(textures != nullptr);
    CHECK(textures->kind == ShaderBindingKind::SampledTexture);
    CHECK(textures->slot == 0);
    CHECK(textures->array_count == 4);
    CHECK(textures->texture_dimension == ShaderTextureDimension::Tex2D);
    CHECK(textures->texture_data_type == ShaderTextureData::Float);
    const ShaderBinding* sampler = find_binding(reflection.parameters, "smp");
    REQUIRE(sampler != nullptr);
    CHECK(sampler->kind == ShaderBindingKind::Sampler);
    CHECK(sampler->array_count == 1);
    CHECK(find_binding(reflection.parameters, "missing") == nullptr);
}

TEST_CASE("MslShaderCompiler reflects every texture variant")
{
    struct Case
    {
        const char* type;
        ShaderBindingKind kind;
        ShaderTextureDimension dimension;
        ShaderTextureData data;
    };
    const Case cases[] = {
        { "texture2d<float>", ShaderBindingKind::SampledTexture, ShaderTextureDimension::Tex2D, ShaderTextureData::Float },
        { "texture2d<half>", ShaderBindingKind::SampledTexture, ShaderTextureDimension::Tex2D, ShaderTextureData::Float },
        { "texture2d<int>", ShaderBindingKind::SampledTexture, ShaderTextureDimension::Tex2D, ShaderTextureData::Int },
        { "texture2d<uint>", ShaderBindingKind::SampledTexture, ShaderTextureDimension::Tex2D, ShaderTextureData::UInt },
        { "depth2d<float>", ShaderBindingKind::SampledTexture, ShaderTextureDimension::Tex2D, ShaderTextureData::Depth },
        { "texture2d_array<float>", ShaderBindingKind::SampledTexture, ShaderTextureDimension::Tex2DArray, ShaderTextureData::Float },
        { "texturecube<float>", ShaderBindingKind::SampledTexture, ShaderTextureDimension::Cube, ShaderTextureData::Float },
        { "texture3d<float>", ShaderBindingKind::SampledTexture, ShaderTextureDimension::Tex3D, ShaderTextureData::Float },
        { "texture2d_ms<float>", ShaderBindingKind::SampledTexture, ShaderTextureDimension::Tex2DMultisample, ShaderTextureData::Float },
        { "texture2d<float, access::write>", ShaderBindingKind::StorageTexture, ShaderTextureDimension::Tex2D, ShaderTextureData::Float },
        { "texture2d<float, access::read_write>", ShaderBindingKind::StorageTexture, ShaderTextureDimension::Tex2D, ShaderTextureData::Float }
    };
    for (const Case& test : cases)
    {
        CAPTURE(test.type);
        const ShaderReflection reflection = compile_msl(ShaderStage::Pixel, "f", fragment_with_params(std::string(test.type) + " t [[texture(2)]]")).reflection;
        REQUIRE(reflection.parameters.size() == 1);
        CHECK(reflection.parameters[0].kind == test.kind);
        CHECK(reflection.parameters[0].texture_dimension == test.dimension);
        CHECK(reflection.parameters[0].texture_data_type == test.data);
        CHECK(reflection.parameters[0].slot == 2);
    }
}

TEST_CASE("MslShaderCompiler classifies buffer bindings")
{
    const ShaderReflection reflection = compile_msl(ShaderStage::Pixel, "f",
        "struct Params { float4 a; };\n"
        + fragment_with_params("constant Params& p [[buffer(0)]], constant Params* u [[buffer(1)]], device float* s [[buffer(2)]], constant float4& v [[buffer(3)]]")).reflection;
    REQUIRE(reflection.parameters.size() == 4);
    CHECK(reflection.parameters[0].kind == ShaderBindingKind::Constants);
    CHECK(reflection.parameters[0].size == 16);
    CHECK(reflection.parameters[1].kind == ShaderBindingKind::UniformBuffer);
    CHECK(reflection.parameters[1].size == 0);
    CHECK(reflection.parameters[2].kind == ShaderBindingKind::StorageBuffer);
    CHECK(reflection.parameters[3].kind == ShaderBindingKind::Constants);
    CHECK(reflection.parameters[3].size == 16);
}

TEST_CASE("MslShaderCompiler lays out padded structs")
{
    const ShaderReflection reflection = compile_msl(ShaderStage::Pixel, "f",
        "struct S { float a; float3 b; float2 c; half d; };\n"
        + fragment_with_params("constant S& s [[buffer(0)]]")).reflection;
    const ShaderBinding& binding = reflection.parameters[0];
    REQUIRE(binding.members.size() == 4);
    CHECK(binding.members[0].offset == 0);
    CHECK(binding.members[1].offset == 16);
    CHECK(binding.members[2].offset == 32);
    CHECK(binding.members[3].offset == 40);
    CHECK(binding.size == 48);
}

TEST_CASE("MslShaderCompiler supports defines, conditionals and macros")
{
    const std::string text =
        "#ifndef COUNT\n#define COUNT 2\n#endif\n"
        "#ifdef FLAT\n#define FLAG 1\n#else\n#define FLAG 0\n#endif\n"
        + fragment_with_params("array<texture2d<float>, COUNT> t [[texture(0)]]");
    CHECK(compile_msl(ShaderStage::Pixel, "f", text).reflection.parameters[0].array_count == 2);
    CHECK(compile_msl(ShaderStage::Pixel, "f", text, { { "COUNT", "8" } }).reflection.parameters[0].array_count == 8);
    const std::string binary = [&] {
        const ShaderCompilerOutput output = compile_msl(ShaderStage::Pixel, "f", text, { { "COUNT", "8" } });
        return std::string(output.binary.begin(), output.binary.end());
    }();
    CHECK(contains(binary, "#define COUNT 8"));
    CHECK(contains(binary, "#define ORYX_MSL 1"));
    CHECK(contains(binary, "#define ORYX_STAGE_PIXEL 1"));
}

TEST_CASE("MslShaderCompiler exposes base definitions to conditionals")
{
    const std::string text =
        "#ifdef ORYX_STAGE_PIXEL\n#define N 3\n#else\n#define N 5\n#endif\n"
        + fragment_with_params("array<texture2d<float>, N> t [[texture(0)]]");
    CHECK(compile_msl(ShaderStage::Pixel, "f", text).reflection.parameters[0].array_count == 3);
}

TEST_CASE("MslShaderCompiler resolves includes")
{
    register_shader_include("test/Part.msl", "struct Shared { float4 value; };\n");
    register_shader_include("test/Outer.msl", "#include \"test/Part.msl\"\n");
    const std::string text = "#include \"test/Outer.msl\"\n#include \"test/Part.msl\"\n" + fragment_with_params("constant Shared& s [[buffer(0)]]");
    const ShaderCompilerOutput output = compile_msl(ShaderStage::Pixel, "f", text);
    CHECK(output.reflection.parameters[0].size == 16);
    const std::string binary(output.binary.begin(), output.binary.end());
    CHECK(binary.find("struct Shared") == binary.rfind("struct Shared"));
}

TEST_CASE("MslShaderCompiler reports include errors with locations")
{
    CHECK(contains(compile_error(ShaderStage::Pixel, "f", "\n#include \"test/Nope.msl\"\n"), "test.msl:2: include 'test/Nope.msl' not found"));

    register_shader_include("test/CycleA.msl", "#include \"test/CycleB.msl\"\n");
    register_shader_include("test/CycleB.msl", "#include \"test/CycleA.msl\"\n");
    CHECK(contains(compile_error(ShaderStage::Pixel, "f", "#include \"test/CycleA.msl\"\n"), "include cycle"));

    register_shader_include("test/Broken.msl", "\n\nstruct B { float5 x; };\n");
    CHECK(contains(compile_error(ShaderStage::Pixel, "f", "#include \"test/Broken.msl\"\n"), "test/Broken.msl:3:"));
}

TEST_CASE("MslShaderCompiler rejects invalid shaders")
{
    SUBCASE("missing entry point")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "nope", SAMPLE_MSL), "entry point 'nope' not found"));
    }
    SUBCASE("wrong entry qualifier")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "vs_main", SAMPLE_MSL), "must be declared 'fragment'"));
        CHECK(contains(compile_error(ShaderStage::Vertex, "ps_main", SAMPLE_MSL), "must be declared 'vertex'"));
    }
    SUBCASE("unsupported stage")
    {
        CHECK(contains(compile_error(ShaderStage::TessEval, "f", fragment_with_params("")), "does not support"));
    }
    SUBCASE("duplicate buffer slot")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", "struct A { float a; };\n" + fragment_with_params("constant A& x [[buffer(1)]], constant A& y [[buffer(1)]]")), "slot 1 of 'y' is already used by 'x'"));
    }
    SUBCASE("duplicate texture slot")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", fragment_with_params("texture2d<float> a [[texture(0)]], texture2d<float> b [[texture(0)]]")), "slot 0"));
    }
    SUBCASE("overlapping texture array slots")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", fragment_with_params("array<texture2d<float>, 4> a [[texture(0)]], texture2d<float> b [[texture(3)]]")), "slot 3"));
    }
    SUBCASE("duplicate sampler slot")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", fragment_with_params("sampler a [[sampler(0)]], sampler b [[sampler(0)]]")), "slot 0"));
    }
    SUBCASE("same slot in different kinds is fine")
    {
        CHECK_NOTHROW(compile_msl(ShaderStage::Pixel, "f", fragment_with_params("texture2d<float> a [[texture(0)]], sampler b [[sampler(0)]]")));
    }
    SUBCASE("duplicate parameter name")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", fragment_with_params("texture2d<float> a [[texture(0)]], sampler a [[sampler(0)]]")), "'a' is declared twice"));
    }
    SUBCASE("duplicate vertex attribute location")
    {
        const std::string text = "struct In { float3 a [[attribute(0)]]; float4 b [[attribute(0)]]; };\n"
                                 "vertex float4 v(In in [[stage_in]]) { return float4(0); }\n";
        CHECK(contains(compile_error(ShaderStage::Vertex, "v", text), "location 0 is used by both 'a' and 'b'"));
    }
    SUBCASE("vertex input without attribute")
    {
        const std::string text = "struct In { float3 a; };\nvertex float4 v(In in [[stage_in]]) { return float4(0); }\n";
        CHECK(contains(compile_error(ShaderStage::Vertex, "v", text), "needs [[attribute(n)]]"));
    }
    SUBCASE("zero array count")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", fragment_with_params("array<texture2d<float>, 0> t [[texture(0)]]")), "must be greater than zero"));
    }
    SUBCASE("non-literal array count")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", fragment_with_params("array<texture2d<float>, N> t [[texture(0)]]")), "expected an integer literal"));
    }
    SUBCASE("vertex without position")
    {
        const std::string text = "struct Out { float4 colour; };\nvertex Out v() { Out o; return o; }\n";
        CHECK(contains(compile_error(ShaderStage::Vertex, "v", text), "must output a [[position]]"));
    }
    SUBCASE("unsupported member type")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", "struct S { float5 a; };\n" + fragment_with_params("")), "unsupported member type 'float5'"));
    }
    SUBCASE("unsupported attribute")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", fragment_with_params("sampler s [[bogus(0)]]")), "unsupported attribute 'bogus'"));
    }
    SUBCASE("missing binding attribute")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", fragment_with_params("sampler s")), "has no binding attribute"));
    }
    SUBCASE("texture attribute on a non-texture")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", fragment_with_params("sampler s [[texture(0)]]")), "is not a texture"));
    }
    SUBCASE("unsupported top-level construct reports its line")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", "\n\nfloat x = 1.0;\n"), "test.msl:3:"));
    }
    SUBCASE("unsupported preprocessor conditional")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", "#if 1\n#endif\n"), "#if is not supported"));
    }
    SUBCASE("unterminated struct")
    {
        CHECK(contains(compile_error(ShaderStage::Pixel, "f", "struct S { float a;"), "unterminated struct"));
    }
}
