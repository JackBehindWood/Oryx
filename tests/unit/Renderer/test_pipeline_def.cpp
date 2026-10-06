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

TEST_CASE("PipelineDef: each primitive's pipeline has its topology, blend and declaration")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    const ShaderLibrary& shaders = Renderer::shaders();
    const RHIFormat format = Renderer::back_buffer_format();

    const GraphicsPipelineDesc triangles = pipeline_desc(pipeline_def(Primitive2D::Triangle), shaders, format);
    const GraphicsPipelineDesc lines = pipeline_desc(pipeline_def(Primitive2D::Line), shaders, format);
    const GraphicsPipelineDesc quad = pipeline_desc(pipeline_def(Primitive2D::Quad), shaders, format);
    const GraphicsPipelineDesc circle = pipeline_desc(pipeline_def(Primitive2D::Circle), shaders, format);
    const GraphicsPipelineDesc text = pipeline_desc(pipeline_def(Primitive2D::Text), shaders, format);
    CHECK(text.state.blend[0].enabled);
    CHECK(text.state.vertex_declaration == vertex_declaration<Vertex2DText>());
    CHECK_NOTHROW(pipeline_desc(pipeline_def(Primitive2D::Text), shaders, format, 1));
    CHECK_THROWS_AS(pipeline_desc(pipeline_def(Primitive2D::Text), shaders, format, 2), Error);
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

TEST_CASE("PipelineDef: primitive traits match the pipeline's topology and vertex size")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    for (uint32_t i = 0; i < PRIMITIVE_2D_COUNT; ++i)
    {
        const PrimitiveTraits traits = primitive_traits(static_cast<Primitive2D>(i));
        const GraphicsPipelineDesc desc = pipeline_desc(pipeline_def(static_cast<Primitive2D>(i)), Renderer::shaders(), Renderer::back_buffer_format());
        CHECK(desc.state.topology == traits.topology);
        CHECK(desc.state.vertex_declaration.stride() == traits.vertex_size);
    }
}

TEST_CASE("PipelineMemo: get caches handles, separates permutations and rebuilds after release")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });

    const GraphicsPipelineHandle quad = Renderer::pipeline(pipeline_def(Primitive2D::Quad));
    CHECK(Renderer::pipeline(pipeline_def(Primitive2D::Quad)) == quad);
    CHECK(Renderer::pipeline(pipeline_def(Primitive2D::Quad), 1) != quad);
    CHECK(Renderer::pipeline(pipeline_def(Primitive2D::Circle)) != quad);
    CHECK(Renderer::pipeline_cache_stats().entries == 3);
    CHECK(Renderer::pipeline(pipeline_def(Primitive2D::Text)) != quad);
    CHECK(Renderer::pipeline(pipeline_def(Primitive2D::Text), 1) != Renderer::pipeline(pipeline_def(Primitive2D::Text)));
    const uint32_t with_text = Renderer::pipeline_cache_stats().entries;
    CHECK(with_text == 5);
    (void)Renderer::pipeline(pipeline_def(Primitive2D::Quad));
    CHECK(Renderer::pipeline_cache_stats().entries == with_text);

    Renderer::release_pipelines();
    CHECK_THROWS_AS(Renderer::resolve_pipeline(quad), Error);
    const GraphicsPipelineHandle rebuilt = Renderer::pipeline(pipeline_def(Primitive2D::Quad));
    CHECK(rebuilt != quad);
    CHECK_NOTHROW(Renderer::resolve_pipeline(rebuilt));

    CHECK_THROWS_AS(Renderer::pipeline(pipeline_def(Primitive2D::Quad), 2), Error);
    CHECK_THROWS_AS(Renderer::pipeline(pipeline_def(Primitive2D::Circle), 1), Error);
}

TEST_CASE("TextureArrayPermutations: fitting picks the largest permutation within the limit")
{
    CHECK(TextureArrayPermutations::fitting(RHI_MAX_TEXTURE_BINDINGS) == 1);
    CHECK(TextureArrayPermutations::fitting(RHI_MAX_TEXTURE_BINDINGS - 1) == 0);
    CHECK(TextureArrayPermutations::fitting(TextureArrayPermutations::DEFAULT_TEXTURES) == 0);
    CHECK_THROWS_AS(TextureArrayPermutations::fitting(TextureArrayPermutations::DEFAULT_TEXTURES - 1), Error);
}
