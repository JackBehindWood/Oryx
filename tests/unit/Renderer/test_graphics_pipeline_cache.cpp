#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"

using namespace oryx;

namespace
{

struct RendererGuard
{
    ~RendererGuard() { Renderer::shutdown(); }
};

GraphicsPipelineDesc base_desc()
{
    return builtin_pipeline_desc(BuiltinPipeline::SolidTriangles, Renderer::shaders(), RHIFormat::BGRA8Unorm);
}

} // namespace

TEST_CASE("GraphicsPipelineCache: equal descriptions share one pipeline")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });

    const GraphicsPipelineHandle first = Renderer::pipeline(base_desc());
    CHECK(Renderer::pipeline_cache_stats().misses == 1);
    CHECK(Renderer::pipeline_cache_stats().hits == 0);

    const GraphicsPipelineHandle second = Renderer::pipeline(base_desc());
    CHECK(first == second);
    CHECK(&Renderer::resolve_pipeline(first) == &Renderer::resolve_pipeline(second));
    CHECK(Renderer::pipeline_cache_stats().hits == 1);
    CHECK(Renderer::pipeline_cache_stats().entries == 1);
}

TEST_CASE("GraphicsPipelineCache: any differing field makes a different pipeline")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    const uint64_t base = hash_graphics_pipeline_desc(base_desc());

    std::vector<std::pair<const char*, GraphicsPipelineDesc>> variants;
    const auto vary = [&](const char* name, auto&& change)
    {
        GraphicsPipelineDesc desc = base_desc();
        change(desc);
        variants.emplace_back(name, std::move(desc));
    };

    vary("pixel shader", [](GraphicsPipelineDesc& d) { d.shaders = builtin_pipeline_desc(BuiltinPipeline::Quad, Renderer::shaders(), RHIFormat::BGRA8Unorm).shaders; });
    vary("permutation", [](GraphicsPipelineDesc& d) { d = builtin_pipeline_desc(BuiltinPipeline::Quad, Renderer::shaders(), RHIFormat::BGRA8Unorm, 1); });
    vary("stride", [](GraphicsPipelineDesc& d) { d.state.vertex_declaration = RHIVertexDeclarationBuilder().stream(0, 32).attribute(0, RHIVertexFormat::Float3, 0).attribute(1, RHIVertexFormat::Float4, 12).build(); });
    vary("attribute location", [](GraphicsPipelineDesc& d) { d.state.vertex_declaration = RHIVertexDeclarationBuilder().stream(0, 28).attribute(5, RHIVertexFormat::Float3, 0).attribute(1, RHIVertexFormat::Float4, 12).build(); });
    vary("attribute format", [](GraphicsPipelineDesc& d) { d.state.vertex_declaration = RHIVertexDeclarationBuilder().stream(0, 28).attribute(0, RHIVertexFormat::Float4, 0).attribute(1, RHIVertexFormat::Float4, 12).build(); });
    vary("attribute offset", [](GraphicsPipelineDesc& d) { d.state.vertex_declaration = RHIVertexDeclarationBuilder().stream(0, 28).attribute(0, RHIVertexFormat::Float3, 4).attribute(1, RHIVertexFormat::Float4, 12).build(); });
    vary("attribute slot", [](GraphicsPipelineDesc& d) { d.state.vertex_declaration = RHIVertexDeclarationBuilder().stream(0, 28).stream(1, 28).attribute(0, RHIVertexFormat::Float3, 0, 1).attribute(1, RHIVertexFormat::Float4, 12).build(); });
    vary("attribute count", [](GraphicsPipelineDesc& d) { d.state.vertex_declaration = RHIVertexDeclarationBuilder().stream(0, 28).attribute(0, RHIVertexFormat::Float3, 0).build(); });
    vary("stream step function", [](GraphicsPipelineDesc& d) { d.state.vertex_declaration = RHIVertexDeclarationBuilder().stream(0, 28, RHIVertexStep::PerInstance).attribute(0, RHIVertexFormat::Float3, 0).attribute(1, RHIVertexFormat::Float4, 12).build(); });
    vary("stream step rate", [](GraphicsPipelineDesc& d) { d.state.vertex_declaration = RHIVertexDeclarationBuilder().stream(0, 28, RHIVertexStep::PerVertex, 2).attribute(0, RHIVertexFormat::Float3, 0).attribute(1, RHIVertexFormat::Float4, 12).build(); });
    vary("topology", [](GraphicsPipelineDesc& d) { d.state.topology = RHITopology::Lines; });
    vary("cull", [](GraphicsPipelineDesc& d) { d.state.rasterizer.cull = RHICullMode::Back; });
    vary("front face", [](GraphicsPipelineDesc& d) { d.state.rasterizer.front_face = RHIFrontFace::Clockwise; });
    vary("fill", [](GraphicsPipelineDesc& d) { d.state.rasterizer.fill = RHIFillMode::Wireframe; });
    vary("blend", [](GraphicsPipelineDesc& d) { d.state.blend[0] = rhi_blend_alpha(); });
    vary("blend op", [](GraphicsPipelineDesc& d) { d.state.blend[0].colour_op = RHIBlendOp::Subtract; });
    vary("blend write mask", [](GraphicsPipelineDesc& d) { d.state.blend[0].write_mask = RHIColourWriteMask::Red; });
    vary("blend slot 1", [](GraphicsPipelineDesc& d) { d.state.blend[1] = rhi_blend_additive(); });
    vary("depth test", [](GraphicsPipelineDesc& d) { d.state.depth_stencil.depth_test = true; });
    vary("depth write", [](GraphicsPipelineDesc& d) { d.state.depth_stencil.depth_write = true; });
    vary("depth compare", [](GraphicsPipelineDesc& d) { d.state.depth_stencil.depth_compare = RHICompare::Greater; });
    vary("stencil test", [](GraphicsPipelineDesc& d) { d.state.depth_stencil.stencil_test = true; });
    vary("stencil front", [](GraphicsPipelineDesc& d) { d.state.depth_stencil.front.pass = RHIStencilOp::Replace; });
    vary("stencil back", [](GraphicsPipelineDesc& d) { d.state.depth_stencil.back.compare = RHICompare::Equal; });
    vary("stencil read mask", [](GraphicsPipelineDesc& d) { d.state.depth_stencil.stencil_read_mask = 0x0F; });
    vary("stencil write mask", [](GraphicsPipelineDesc& d) { d.state.depth_stencil.stencil_write_mask = 0x0F; });
    vary("colour format", [](GraphicsPipelineDesc& d) { d.state.colour_formats[0] = RHIFormat::RGBA8Unorm; });
    vary("colour format count", [](GraphicsPipelineDesc& d) { d.state.colour_format_count = 2; });
    vary("depth format", [](GraphicsPipelineDesc& d) { d.state.depth_format = RHIFormat::Depth32Float; });
    vary("sample count", [](GraphicsPipelineDesc& d) { d.state.sample_count = 4; });

    for (const std::pair<const char*, GraphicsPipelineDesc>& variant : variants)
    {
        CAPTURE(variant.first);
        CHECK(hash_graphics_pipeline_desc(variant.second) != base);
    }
}

TEST_CASE("GraphicsPipelineCache: the key does not depend on addresses or the run")
{
    uint64_t first = 0;
    {
        RendererGuard guard;
        Renderer::init({ RHIBackend::Null });
        first = hash_graphics_pipeline_desc(base_desc());
    }
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    CHECK(hash_graphics_pipeline_desc(base_desc()) == first);
    CHECK_THROWS_AS(hash_graphics_pipeline_desc({}), Error);
}

TEST_CASE("GraphicsPipelineCache: built-in descriptions are distinct and stable")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    const ShaderLibrary& shaders = Renderer::shaders();
    const RHIFormat format = Renderer::back_buffer_format();

    const GraphicsPipelineHandle triangles = Renderer::pipeline(builtin_pipeline_desc(BuiltinPipeline::SolidTriangles, shaders, format));
    const GraphicsPipelineHandle lines = Renderer::pipeline(builtin_pipeline_desc(BuiltinPipeline::SolidLines, shaders, format));
    const GraphicsPipelineHandle quad = Renderer::pipeline(builtin_pipeline_desc(BuiltinPipeline::Quad, shaders, format));
    const GraphicsPipelineHandle circle = Renderer::pipeline(builtin_pipeline_desc(BuiltinPipeline::Circle, shaders, format));
    CHECK(Renderer::pipeline_cache_stats().entries == 4);
    CHECK(triangles != lines);
    CHECK(quad != circle);
    CHECK(triangles != quad);

    CHECK(Renderer::pipeline(builtin_pipeline_desc(BuiltinPipeline::Quad, shaders, format)) == quad);
    CHECK(Renderer::pipeline(builtin_pipeline_desc(BuiltinPipeline::Quad, shaders, RHIFormat::RGBA8Unorm)) != quad);
    CHECK(Renderer::pipeline(builtin_pipeline_desc(BuiltinPipeline::Quad, shaders, format, 1)) != quad);
}

TEST_CASE("GraphicsPipelineCache: addresses stay stable while more pipelines are added")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    const ShaderLibrary& shaders = Renderer::shaders();
    const GraphicsPipelineHandle first = Renderer::pipeline(builtin_pipeline_desc(BuiltinPipeline::SolidTriangles, shaders, RHIFormat::BGRA8Unorm));
    const GraphicsPipeline* address = &Renderer::resolve_pipeline(first);
    for (const RHICullMode cull : { RHICullMode::Front, RHICullMode::Back })
    {
        for (const RHIFrontFace face : { RHIFrontFace::Clockwise, RHIFrontFace::CounterClockwise })
        {
            GraphicsPipelineDesc desc = builtin_pipeline_desc(BuiltinPipeline::SolidTriangles, shaders, RHIFormat::BGRA8Unorm);
            desc.state.rasterizer.cull = cull;
            desc.state.rasterizer.front_face = face;
            (void)Renderer::pipeline(desc);
        }
    }
    CHECK(Renderer::pipeline_cache_stats().entries == 5);
    CHECK(&Renderer::resolve_pipeline(first) == address);
}

TEST_CASE("GraphicsPipelineCache: invalid and stale handles throw")
{
    GraphicsPipelineCache cache;
    CHECK_THROWS_AS(cache.resolve({}), Error);
    CHECK(cache.generation() == 1);
    cache.clear();
    CHECK(cache.generation() == 2);
    CHECK_THROWS_AS(cache.resolve({ 0, 1 }), Error);
    CHECK_THROWS_AS(cache.resolve({ 0, 2 }), Error);
}
