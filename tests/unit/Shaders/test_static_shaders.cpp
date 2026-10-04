#include "doctest.h"

#include "ShaderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

uint32_t layout_extent(const VertexLayout& layout)
{
    uint32_t extent = 0;
    for (const RHIVertexAttribute& attribute : layout.attributes)
    {
        extent = std::max(extent, attribute.offset + rhi_vertex_format_bytes(attribute.format));
    }
    return extent;
}

struct StaticFixture
{
    NullRHI rhi;
    ShaderCache cache;
    ShaderLibrary library;

    StaticFixture()
    {
        library.compile(rhi, cache, shader_type_of<SolidVS>());
        library.compile(rhi, cache, shader_type_of<SolidPS>());
        library.compile(rhi, cache, shader_type_of<QuadVS>());
        library.compile(rhi, cache, shader_type_of<QuadPS>());
        library.compile(rhi, cache, shader_type_of<CircleVS>());
        library.compile(rhi, cache, shader_type_of<CirclePS>());
    }
};

void check_frame_binding(const Shader& shader)
{
    const ShaderBinding& frame = shader.binding(SHADER_FRAME_BINDING);
    CHECK(frame.kind == ShaderBindingKind::Constants);
    CHECK(frame.slot == 0);
    CHECK(frame.size == SHADER_FRAME_SIZE);
    REQUIRE(frame.members.size() == 1);
    CHECK(frame.members[0].name == "view_projection");
    CHECK(frame.members[0].size == 64);
}

} // namespace

TEST_CASE("Built-in shaders compile and are registered")
{
    StaticFixture f;
    CHECK(f.library.contains<SolidVS>());
    CHECK(f.library.contains<SolidPS>());
    CHECK(f.library.contains<QuadVS>());
    CHECK(f.library.contains<QuadPS>(0));
    CHECK(f.library.contains<QuadPS>(1));
    CHECK_FALSE(f.library.contains<QuadPS>(2));
    CHECK(f.library.contains<CircleVS>());
    CHECK(f.library.contains<CirclePS>());
    CHECK(f.library.get<SolidVS>()->stage() == ShaderStage::Vertex);
    CHECK(f.library.get<SolidPS>()->rhi()->stage() == RHIShaderStage::Pixel);
}

TEST_CASE("Built-in vertex shaders share the frame constants and match their layouts")
{
    StaticFixture f;
    check_frame_binding(*f.library.get<SolidVS>());
    check_frame_binding(*f.library.get<QuadVS>());
    check_frame_binding(*f.library.get<CircleVS>());

    CHECK(f.library.get<SolidVS>()->reflection().inputs.size() == solid_vertex_layout().attributes.size());
    CHECK(f.library.get<QuadVS>()->reflection().inputs.size() == quad_vertex_layout().attributes.size());
    CHECK(f.library.get<CircleVS>()->reflection().inputs.size() == circle_vertex_layout().attributes.size());
    CHECK(solid_vertex_layout().stride == 28);
    CHECK(quad_vertex_layout().stride == 40);
    CHECK(circle_vertex_layout().stride == 44);
    CHECK(layout_extent(solid_vertex_layout()) == solid_vertex_layout().stride);
    CHECK(layout_extent(quad_vertex_layout()) == quad_vertex_layout().stride);
    CHECK(layout_extent(circle_vertex_layout()) == circle_vertex_layout().stride);
}

TEST_CASE("Quad vertex and pixel shaders reflect the texture array")
{
    StaticFixture f;
    const ShaderReflection& vertex = f.library.get<QuadVS>()->reflection();
    REQUIRE(vertex.inputs.size() == 4);
    CHECK(vertex.inputs[3].name == "tex_index");
    CHECK(vertex.inputs[3].location == 3);

    for (uint32_t permutation = 0; permutation < 2; ++permutation)
    {
        const Ref<QuadPS> pixel = f.library.get<QuadPS>(permutation);
        const ShaderBinding& textures = pixel->binding("textures");
        CHECK(textures.kind == ShaderBindingKind::SampledTexture);
        CHECK(textures.slot == 0);
        CHECK(textures.array_count == QuadPS::texture_count(permutation));
        CHECK(textures.texture_dimension == ShaderTextureDimension::Tex2D);
        CHECK(pixel->binding("smp").kind == ShaderBindingKind::Sampler);
        CHECK(pixel->permutation() == permutation);
    }
    CHECK(f.library.get<QuadPS>(0)->binding("textures").array_count == 16);
    CHECK(f.library.get<QuadPS>(1)->binding("textures").array_count == 32);
    CHECK(f.library.get<QuadPS>(0)->hash() != f.library.get<QuadPS>(1)->hash());
}

TEST_CASE("Circle shaders carry thickness and fade to the pixel stage")
{
    StaticFixture f;
    const ShaderReflection& vertex = f.library.get<CircleVS>()->reflection();
    REQUIRE(vertex.inputs.size() == 5);
    CHECK(vertex.inputs[2].name == "local_position");
    CHECK(vertex.inputs[3].name == "thickness");
    CHECK(vertex.inputs[4].name == "fade");
    CHECK(f.library.get<CirclePS>()->reflection().parameters.empty());
}

TEST_CASE("Built-in sets validate and create pipelines")
{
    StaticFixture f;
    GraphicsPipelineState state;

    state.vertex_layout = solid_vertex_layout();
    GraphicsShaderSet solid{ f.library.get<SolidVS>(), f.library.get<SolidPS>() };
    CHECK_NOTHROW(validate_graphics_shader_set(solid));
    GraphicsPipeline solid_pipeline = make_graphics_pipeline(f.rhi, solid, state);
    CHECK(solid_pipeline.binding_count() == 1);

    state.vertex_layout = quad_vertex_layout();
    GraphicsShaderSet quad{ f.library.get<QuadVS>(), f.library.get<QuadPS>() };
    GraphicsPipeline quad_pipeline = make_graphics_pipeline(f.rhi, quad, state);
    CHECK(quad_pipeline.binding_count() == 3);

    state.vertex_layout = circle_vertex_layout();
    GraphicsShaderSet circle{ f.library.get<CircleVS>(), f.library.get<CirclePS>() };
    CHECK_NOTHROW(make_graphics_pipeline(f.rhi, circle, state));

    state.vertex_layout = solid_vertex_layout();
    CHECK_THROWS_AS(make_graphics_pipeline(f.rhi, quad, state), Error);
}

TEST_CASE("Shader binding lookup throws for unknown names")
{
    StaticFixture f;
    CHECK_THROWS_AS(f.library.get<SolidVS>()->binding("nope"), Error);
}
