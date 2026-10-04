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

    Pipeline solid()
    {
        PipelineState state;
        state.vertex_layout = solid_vertex_layout();
        state.colour_formats[0] = RHIFormat::RGBA8Unorm;
        return make_pipeline(rhi, { library.get<SolidVS>(), library.get<SolidPS>() }, state);
    }

    Pipeline quad()
    {
        PipelineState state;
        state.vertex_layout = quad_vertex_layout();
        state.colour_formats[0] = RHIFormat::RGBA8Unorm;
        state.blend[0] = rhi_blend_alpha();
        return make_pipeline(rhi, { library.get<QuadVS>(), library.get<QuadPS>() }, state);
    }

    void begin(const Pipeline& pipeline)
    {
        list.begin_pass(target.get());
        list.set_pipeline(&pipeline.rhi());
        list.set_vertex_buffer(0, vertices.get());
    }
};

} // namespace

TEST_CASE("Pipeline resolves binding names to ids")
{
    PipelineFixture f;
    const Pipeline solid = f.solid();
    CHECK(solid.binding_count() == 1);
    CHECK(solid.binding("frame") == 0);
    CHECK(solid.rhi().binding(0).kind == RHIBindingKind::Constants);
    CHECK(solid.rhi().binding(0).size == 64);

    const Pipeline quad = f.quad();
    CHECK(quad.binding_count() == 3);
    CHECK(quad.binding("frame") == 0);
    CHECK(quad.binding("textures") == 1);
    CHECK(quad.binding("smp") == 2);
    CHECK(quad.rhi().binding(quad.binding("textures")).array_count == 16);
    CHECK(quad.rhi().binding(quad.binding("frame")).stage_mask == (RHIShaderStageMask::Vertex));
}

TEST_CASE("Pipeline reports unknown binding names")
{
    PipelineFixture f;
    const Pipeline solid = f.solid();
    CHECK_THROWS_WITH_AS(solid.binding("textures"), doctest::Contains("no binding named 'textures'"), Error);
    CHECK(solid.try_binding("textures") == RHI_INVALID_BINDING);
    CHECK(solid.try_binding("frame") == 0);
}

TEST_CASE("Pipeline construction validates its inputs")
{
    PipelineFixture f;
    const Pipeline solid = f.solid();
    CHECK_THROWS_AS(Pipeline(nullptr, {}), Error);
    CHECK_THROWS_AS(Pipeline(solid.rhi_ptr(), {}), Error);
}

TEST_CASE("Command lists validate against a real pipeline")
{
    PipelineFixture f;
    const Pipeline quad = f.quad();
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
    const Pipeline solid = f.solid();
    const Pipeline quad = f.quad();
    const std::array<float, 16> matrix = {};

    f.list.begin_pass(f.target.get(), { Colour{ 0.0f, 0.0f, 0.0f, 1.0f }, true });
    f.list.set_pipeline(&solid.rhi());
    f.list.set_vertex_buffer(0, f.vertices.get());
    f.list.set_constants(solid.binding("frame"), matrix.data(), sizeof(matrix));
    f.list.draw(3);
    f.list.set_pipeline(&quad.rhi());
    f.list.set_vertex_buffer(0, f.vertices.get());
    f.list.set_constants(quad.binding("frame"), matrix.data(), sizeof(matrix));
    f.list.bind_texture(quad.binding("textures"), f.texture.get(), 0);
    f.list.bind_sampler(quad.binding("smp"), f.sampler.get());
    f.list.draw(6);
    f.list.end_pass();

    const size_t commands = f.list.size();
    f.rhi.submit(f.list);
    f.rhi.end_frame();
    CHECK(f.rhi.submit_count() == 1);
    CHECK(f.rhi.last_submission().size() == commands);
}
