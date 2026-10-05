#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

namespace
{

struct RendererGuard
{
    ~RendererGuard() { Renderer::shutdown(); }
};

} // namespace

TEST_CASE("BuiltinPipelines: each pipeline has its effect's topology, blend and declaration")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    const ShaderLibrary& shaders = Renderer::shaders();
    const RHIFormat format = Renderer::back_buffer_format();

    const GraphicsPipelineDesc triangles = builtin_pipeline_desc(BuiltinPipeline::SolidTriangles, shaders, format);
    const GraphicsPipelineDesc lines = builtin_pipeline_desc(BuiltinPipeline::SolidLines, shaders, format);
    const GraphicsPipelineDesc quad = builtin_pipeline_desc(BuiltinPipeline::Quad, shaders, format);
    const GraphicsPipelineDesc circle = builtin_pipeline_desc(BuiltinPipeline::Circle, shaders, format);
    CHECK(triangles.state.topology == RHITopology::Triangles);
    CHECK(lines.state.topology == RHITopology::Lines);
    CHECK_FALSE(triangles.state.blend[0].enabled);
    CHECK(quad.state.blend[0].enabled);
    CHECK(circle.state.blend[0].enabled);
    CHECK(quad.state.vertex_declaration == vertex_declaration<Vertex2DQuad>());
    CHECK(circle.state.vertex_declaration == vertex_declaration<Vertex2DCircle>());
    CHECK(triangles.state.vertex_declaration == lines.state.vertex_declaration);
    CHECK(triangles.state.colour_formats[0] == format);
}

TEST_CASE("BuiltinPipelines: primitive traits name the pipeline with the matching topology")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    for (uint32_t i = 0; i < PRIMITIVE_2D_COUNT; ++i)
    {
        const PrimitiveTraits traits = primitive_traits(static_cast<Primitive2D>(i));
        const GraphicsPipelineDesc desc = builtin_pipeline_desc(traits.pipeline, Renderer::shaders(), Renderer::back_buffer_format());
        CHECK(desc.state.topology == traits.topology);
        CHECK(desc.state.vertex_declaration.stride() == traits.vertex_size);
    }
}

TEST_CASE("BuiltinPipelines: get caches handles, separates permutations and rebuilds after release")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });

    const GraphicsPipelineHandle quad = Renderer::builtin(BuiltinPipeline::Quad);
    CHECK(Renderer::builtin(BuiltinPipeline::Quad) == quad);
    CHECK(Renderer::builtin(BuiltinPipeline::Quad, 1) != quad);
    CHECK(Renderer::builtin(BuiltinPipeline::Circle) != quad);
    const uint32_t entries = Renderer::pipeline_cache_stats().entries;
    CHECK(entries == 3);
    (void)Renderer::builtin(BuiltinPipeline::Quad);
    CHECK(Renderer::pipeline_cache_stats().entries == entries);

    Renderer::release_pipelines();
    CHECK_THROWS_AS(Renderer::resolve_pipeline(quad), Error);
    const GraphicsPipelineHandle rebuilt = Renderer::builtin(BuiltinPipeline::Quad);
    CHECK(rebuilt != quad);
    CHECK_NOTHROW(Renderer::resolve_pipeline(rebuilt));

    CHECK_THROWS_AS(Renderer::builtin(BuiltinPipeline::Quad, BUILTIN_MAX_PERMUTATIONS), Error);
    CHECK_THROWS_AS(Renderer::builtin(BuiltinPipeline::Circle, 1), Error);
}
