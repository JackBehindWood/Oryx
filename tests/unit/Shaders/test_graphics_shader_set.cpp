#include "doctest.h"

#include "ShaderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

std::string vertex_source(const char* varyings, const char* frame_type = "Frame")
{
    return std::string("struct Frame { float4x4 m; };\nstruct Small { float4 v; };\nstruct VIn { float3 p [[attribute(0)]]; };\n")
        + "struct VOut { float4 position [[position]]; " + varyings + " };\n"
        + "vertex VOut vs(VIn in [[stage_in]], constant " + frame_type + "& frame [[buffer(0)]]) { VOut o; return o; }\n";
}

std::string pixel_source(const char* varyings, const char* extra = "", const char* frame_type = "Frame")
{
    return std::string("struct Frame { float4x4 m; };\nstruct Small { float4 v; };\n")
        + "struct PIn { float4 position [[position]]; " + varyings + " };\n"
        + "fragment float4 ps(PIn in [[stage_in]], constant " + frame_type + "& frame [[buffer(0)]]" + extra + ") { return float4(0); }\n";
}

struct SetFixture
{
    NullRHI rhi;
    ShaderCache cache;

    GraphicsShaderSet make(const std::string& vertex, const std::string& pixel)
    {
        return { make_vertex_shader(rhi, cache, vertex, "vs"), make_pixel_shader(rhi, cache, pixel, "ps") };
    }
};

} // namespace

TEST_CASE("validate_graphics_shader_set accepts matching interfaces")
{
    SetFixture f;
    CHECK_NOTHROW(validate_graphics_shader_set(f.make(vertex_source("float4 colour; float2 uv;"), pixel_source("float4 colour;"))));
    CHECK_NOTHROW(validate_graphics_shader_set(f.make(vertex_source("float4 colour;"), pixel_source(""))));
}

TEST_CASE("validate_graphics_shader_set rejects missing stages")
{
    SetFixture f;
    GraphicsShaderSet set = f.make(vertex_source(""), pixel_source(""));
    GraphicsShaderSet no_pixel{ set.vertex, nullptr };
    GraphicsShaderSet no_vertex{ nullptr, set.pixel };
    CHECK_THROWS_AS(validate_graphics_shader_set(no_pixel), Error);
    CHECK_THROWS_AS(validate_graphics_shader_set(no_vertex), Error);
}

TEST_CASE("validate_graphics_shader_set rejects interface mismatches")
{
    SetFixture f;
    SUBCASE("pixel input without a vertex output")
    {
        const GraphicsShaderSet set = f.make(vertex_source("float4 colour;"), pixel_source("float2 uv;"));
        CHECK_THROWS_WITH_AS(validate_graphics_shader_set(set), doctest::Contains("'uv' has no matching output"), Error);
    }
    SUBCASE("type mismatch")
    {
        const GraphicsShaderSet set = f.make(vertex_source("float2 colour;"), pixel_source("float4 colour;"));
        CHECK_THROWS_WITH_AS(validate_graphics_shader_set(set), doctest::Contains("float4 but the vertex shader outputs float2"), Error);
    }
    SUBCASE("conflicting binding between stages")
    {
        const GraphicsShaderSet set = f.make(vertex_source(""), pixel_source("", "", "Small"));
        CHECK_THROWS_WITH_AS(validate_graphics_shader_set(set), doctest::Contains("'frame' differs"), Error);
    }
}

TEST_CASE("to_rhi_binding_layout merges stages by name")
{
    SetFixture f;
    const GraphicsShaderSet set = f.make(vertex_source(""), pixel_source("", ", array<texture2d<float>, 4> tex [[texture(0)]], sampler smp [[sampler(1)]]"));
    const ShaderBindingLayout layout = to_rhi_binding_layout(set.vertex->reflection(), set.pixel->reflection());

    REQUIRE(layout.names.size() == 3);
    CHECK(layout.names[0] == "frame");
    CHECK(layout.names[1] == "tex");
    CHECK(layout.names[2] == "smp");

    CHECK(layout.bindings[0].kind == RHIBindingKind::Constants);
    CHECK(layout.bindings[0].size == 64);
    CHECK((layout.bindings[0].stage_mask & RHIShaderStageMask::Vertex) == RHIShaderStageMask::Vertex);
    CHECK((layout.bindings[0].stage_mask & RHIShaderStageMask::Pixel) == RHIShaderStageMask::Pixel);

    CHECK(layout.bindings[1].kind == RHIBindingKind::SampledTexture);
    CHECK(layout.bindings[1].array_count == 4);
    CHECK(layout.bindings[1].stage_mask == RHIShaderStageMask::Pixel);
    CHECK(layout.bindings[1].data_type == RHIDataType::Float);
    CHECK(layout.bindings[2].kind == RHIBindingKind::Sampler);
    CHECK(layout.bindings[2].slot == 1);
}

TEST_CASE("to_rhi_binding_layout requires a vertex and a pixel reflection")
{
    SetFixture f;
    const GraphicsShaderSet set = f.make(vertex_source(""), pixel_source(""));
    CHECK_THROWS_AS(to_rhi_binding_layout(set.pixel->reflection(), set.vertex->reflection()), Error);
}

TEST_CASE("Shaders map to RHI enums")
{
    CHECK(to_rhi_stage(ShaderStage::Vertex) == RHIShaderStage::Vertex);
    CHECK(to_rhi_stage(ShaderStage::TessEval) == RHIShaderStage::TessEval);
    CHECK(to_rhi_stage_mask(ShaderStage::Pixel) == RHIShaderStageMask::Pixel);
    CHECK_THROWS_AS(to_rhi_stage_mask(ShaderStage::Compute), Error);
    CHECK(to_rhi_binding_kind(ShaderBindingKind::StorageTexture) == RHIBindingKind::StorageTexture);
    CHECK(to_rhi_data_type(ShaderTextureData::Depth) == RHIDataType::Depth);
    CHECK(to_rhi_texture_dimension(ShaderTextureDimension::Cube) == RHITextureDimension::Cube);
    CHECK(to_rhi_vertex_format({ ShaderScalar::Float, 3, 1 }) == RHIVertexFormat::Float3);
    CHECK(to_rhi_vertex_format({ ShaderScalar::Float, 1, 1 }) == RHIVertexFormat::Float);
    CHECK_THROWS_AS(to_rhi_vertex_format({ ShaderScalar::Int, 2, 1 }), Error);
    CHECK_THROWS_AS(to_rhi_vertex_format({ ShaderScalar::Float, 4, 4 }), Error);
}

TEST_CASE("make_graphics_pipeline checks the vertex layout against the shader inputs")
{
    SetFixture f;
    const GraphicsShaderSet set = f.make(vertex_source("float4 colour;"), pixel_source("float4 colour;"));
    GraphicsPipelineState state;
    state.vertex_layout = { { { 0, RHIVertexFormat::Float3, 0, 0 } }, 12 };
    CHECK_NOTHROW(make_graphics_pipeline(f.rhi, set, state));

    state.vertex_layout = { {}, 12 };
    CHECK_THROWS_WITH_AS(make_graphics_pipeline(f.rhi, set, state), doctest::Contains("no attribute for shader input 'p'"), Error);

    state.vertex_layout = { { { 0, RHIVertexFormat::Float2, 0, 0 } }, 8 };
    CHECK_THROWS_WITH_AS(make_graphics_pipeline(f.rhi, set, state), doctest::Contains("does not match shader input 'p'"), Error);
}
