#include "doctest.h"

#include "ShaderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct PipelineFixture
{
    NullRHI rhi;
    ShaderCache cache;
    ShaderLibrary library;
    RHITexturePtr texture = rhi.create_texture({ .width = 4, .height = 4, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    RHIBufferPtr vertices = rhi.create_buffer({ .size = 4096, .usage = RHIBufferUsage::Vertex });
    RHISamplerPtr sampler = rhi.create_sampler({});
    RHICommandList list;

    PipelineFixture()
    {
        library.compile(rhi, cache, shader_type_of<SolidVS>());
        library.compile(rhi, cache, shader_type_of<SolidPS>());
        library.compile(rhi, cache, shader_type_of<QuadVS>());
        library.compile(rhi, cache, shader_type_of<QuadPS>());
    }

    GraphicsPipeline solid()
    {
        GraphicsPipelineState state;
        state.vertex_declaration = vertex_declaration<Vertex2DLine>();
        state.colour_formats[0] = RHIFormat::RGBA8Unorm;
        return make_graphics_pipeline(rhi, { library.get<SolidVS>(), library.get<SolidPS>() }, state);
    }

    GraphicsPipeline quad()
    {
        GraphicsPipelineState state;
        state.vertex_declaration = vertex_declaration<Vertex2DQuad>();
        state.colour_formats[0] = RHIFormat::RGBA8Unorm;
        state.blend[0] = rhi_blend_alpha();
        return make_graphics_pipeline(rhi, { library.get<QuadVS>(), library.get<QuadPS>() }, state);
    }

    void begin(const GraphicsPipeline& pipeline)
    {
        list.begin_pass(target.get());
        list.set_pipeline(&pipeline.rhi());
        list.set_vertex_buffer(0, vertices.get());
    }
};

} // namespace

TEST_CASE("GraphicsPipeline resolves binding names to ids")
{
    PipelineFixture f;
    const GraphicsPipeline solid = f.solid();
    CHECK(solid.binding_count() == 1);
    CHECK(solid.binding("frame") == 0);
    CHECK(solid.rhi().binding(0).kind == RHIBindingKind::Constants);
    CHECK(solid.rhi().binding(0).size == 64);

    const GraphicsPipeline quad = f.quad();
    CHECK(quad.binding_count() == 3);
    CHECK(quad.binding("frame") == 0);
    CHECK(quad.binding("textures") == 1);
    CHECK(quad.binding("smp") == 2);
    CHECK(quad.rhi().binding(quad.binding("textures")).array_count == 16);
    CHECK(quad.rhi().binding(quad.binding("frame")).stage_mask == (RHIShaderStageMask::Vertex));
}

TEST_CASE("GraphicsPipeline reports unknown binding names")
{
    PipelineFixture f;
    const GraphicsPipeline solid = f.solid();
    CHECK_THROWS_WITH_AS(solid.binding("textures"), doctest::Contains("no binding named 'textures'"), Error);
    CHECK(solid.try_binding("textures") == RHI_INVALID_BINDING);
    CHECK(solid.try_binding("frame") == 0);
}

TEST_CASE("GraphicsPipeline construction validates its inputs")
{
    PipelineFixture f;
    const GraphicsPipeline solid = f.solid();
    CHECK_THROWS_AS(GraphicsPipeline(nullptr, {}), Error);
    CHECK_THROWS_AS(GraphicsPipeline(solid.rhi_ptr(), {}), Error);
}

TEST_CASE("Command lists validate against a real pipeline")
{
    PipelineFixture f;
    const GraphicsPipeline quad = f.quad();
    const RHIBindingId frame = quad.binding("frame");
    const RHIBindingId textures = quad.binding("textures");
    const RHIBindingId smp = quad.binding("smp");
    f.begin(quad);

    const std::array<float, 16> matrix = {};
    CHECK_NOTHROW(f.list.set_constants(frame, matrix.data(), sizeof(matrix)));
    CHECK_THROWS_AS(f.list.set_constants(frame, matrix.data(), 32 * sizeof(float)), Error);
    CHECK_THROWS_AS(f.list.set_constants(textures, matrix.data(), sizeof(matrix)), Error);

    CHECK_NOTHROW(f.list.bind_texture(textures, f.texture.get(), 0));
    CHECK_NOTHROW(f.list.bind_texture(textures, f.texture.get(), 15));
    CHECK_THROWS_AS(f.list.bind_texture(textures, f.texture.get(), 16), Error);
    CHECK_THROWS_AS(f.list.bind_texture(smp, f.texture.get(), 0), Error);
    CHECK_THROWS_AS(f.list.bind_buffer(textures, f.vertices.get(), 0, 16), Error);
    CHECK_NOTHROW(f.list.bind_sampler(smp, f.sampler.get()));
    CHECK_THROWS_AS(f.list.bind_sampler(frame, f.sampler.get()), Error);
    CHECK_THROWS_AS(f.list.bind_sampler(99, f.sampler.get()), Error);
}

TEST_CASE("A frame recorded with real pipelines is accepted by NullRHI")
{
    PipelineFixture f;
    const GraphicsPipeline solid = f.solid();
    const GraphicsPipeline quad = f.quad();
    const std::array<float, 16> matrix = {};

    f.list.begin_pass(f.target.get(), { Colour{ 0.0f, 0.0f, 0.0f, 1.0f }, true });
    f.list.set_pipeline(&solid.rhi());
    f.list.set_vertex_buffer(0, f.vertices.get());
    f.list.set_constants(solid.binding("frame"), matrix.data(), sizeof(matrix));
    f.list.draw(3);
    f.list.set_pipeline(&quad.rhi());
    f.list.set_vertex_buffer(0, f.vertices.get());
    f.list.set_constants(quad.binding("frame"), matrix.data(), sizeof(matrix));
    for (uint32_t i = 0; i < quad.rhi().binding(quad.binding("textures")).array_count; ++i)
    {
        f.list.bind_texture(quad.binding("textures"), f.texture.get(), i);
    }
    f.list.bind_sampler(quad.binding("smp"), f.sampler.get());
    f.list.draw(6);
    f.list.end_pass();

    const size_t commands = f.list.size();
    f.rhi.submit(f.list);
    f.rhi.end_frame();
    CHECK(f.rhi.submit_count() == 1);
    CHECK(f.rhi.last_submission().size() == commands);
}

TEST_CASE("A pipeline with two vertex streams records both buffers")
{
    PipelineFixture f;
    GraphicsPipelineState state;
    state.vertex_declaration = RHIVertexDeclarationBuilder()
                                   .stream(0, 12)
                                   .stream(1, 16, RHIVertexStep::PerInstance)
                                   .attribute(0, RHIVertexFormat::Float3, 0, 0)
                                   .attribute(1, RHIVertexFormat::Float4, 0, 1)
                                   .build();
    state.colour_formats[0] = RHIFormat::RGBA8Unorm;
    const GraphicsPipeline split = make_graphics_pipeline(f.rhi, { f.library.get<SolidVS>(), f.library.get<SolidPS>() }, state);
    RHIBufferPtr colours = f.rhi.create_buffer({ .size = 256, .usage = RHIBufferUsage::Vertex });

    f.begin(split);
    f.list.set_vertex_buffer(1, colours.get(), 16);
    const std::array<float, 16> matrix = {};
    f.list.set_constants(split.binding("frame"), matrix.data(), sizeof(matrix));
    f.list.draw(3, 4);
    f.list.end_pass();
    f.rhi.submit(f.list);

    uint32_t vertex_binds = 0;
    for (const std::string_view type : f.rhi.last_submission())
    {
        vertex_binds += type == "SetVertexBuffer" ? 1 : 0;
    }
    CHECK(vertex_binds == 2);
}
