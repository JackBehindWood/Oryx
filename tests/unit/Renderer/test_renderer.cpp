#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "Oryx/Renderer/FrameRecorder.h"

using namespace oryx;

namespace
{

struct RendererGuard
{
    ~RendererGuard() { Renderer::shutdown(); }
};

struct DrawFixture
{
    NullRHI& rhi;
    RHIViewportPtr viewport;

    DrawFixture()
        : rhi(static_cast<NullRHI&>(Renderer::rhi()))
    {
        viewport = rhi.create_viewport({ .width = 8, .height = 8 });
        Renderer::set_viewport(viewport);
    }

    ~DrawFixture() { Renderer::set_viewport({}); }

    GraphicsPipelineHandle solid() { return Renderer::pipeline(builtin_pipeline_desc(BuiltinPipeline::SolidTriangles, Renderer::shaders(), Renderer::back_buffer_format())); }
    GraphicsPipelineHandle quad() { return Renderer::pipeline(builtin_pipeline_desc(BuiltinPipeline::Quad, Renderer::shaders(), Renderer::back_buffer_format())); }

    DrawItem solid_item(GraphicsPipelineHandle pipeline)
    {
        DrawItem item;
        item.pipeline = pipeline;
        item.vertex_buffers[0] = rhi.create_buffer({ .size = 256, .usage = RHIBufferUsage::Vertex });
        item.vertex_count = 3;
        const float matrix[16] = {};
        draw_item_set_constants(item, matrix);
        return item;
    }
};

} // namespace

TEST_CASE("Renderer: init, frame and shutdown")
{
    RendererGuard guard;
    CHECK_FALSE(Renderer::initialised());
    CHECK_THROWS_AS(Renderer::rhi(), Error);
    CHECK_THROWS_AS(Renderer::set_clear_colour({}), Error);
    CHECK_THROWS_AS(Renderer::submit({}), Error);

    Renderer::init({ RHIBackend::Null });
    CHECK(Renderer::initialised());
    CHECK_THROWS_AS(Renderer::init({ RHIBackend::Null }), Error);
    CHECK(Renderer::rhi().backend() == RHIBackend::Null);

    NullRHI& rhi = static_cast<NullRHI&>(Renderer::rhi());
    Renderer::end_frame();
    CHECK(rhi.submit_count() == 0);

    RHIViewportPtr viewport = rhi.create_viewport({ .width = 8, .height = 8 });
    Renderer::set_viewport(viewport);
    Renderer::set_clear_colour(Colour{ 1.0f, 0.0f, 0.0f, 1.0f });
    CHECK(Renderer::clear_colour() == Colour{ 1.0f, 0.0f, 0.0f, 1.0f });
    Renderer::end_frame();
    CHECK(rhi.submit_count() == 1);
    CHECK(rhi.last_submission() == std::vector<RHICommandType>{ RHICommandType::BeginPass, RHICommandType::EndPass });

    viewport.reset();
    Renderer::set_viewport({});
    Renderer::shutdown();
    CHECK_FALSE(Renderer::initialised());
    Renderer::shutdown();
}

TEST_CASE("Renderer: zero-size viewport drops the frame")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    NullRHI& rhi = static_cast<NullRHI&>(Renderer::rhi());
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 0, .height = 0 });
    Renderer::set_viewport(viewport);
    Renderer::end_frame();
    CHECK(rhi.submit_count() == 0);
    Renderer::set_viewport({});
}

TEST_CASE("Renderer: submitted draws are recorded in order inside one pass")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    DrawFixture fixture;
    GraphicsPipelineHandle solid = fixture.solid();

    Renderer::submit(fixture.solid_item(solid));
    Renderer::submit(fixture.solid_item(solid));
    Renderer::end_frame();

    const std::vector<RHICommandType> draw = { RHICommandType::SetPipeline, RHICommandType::SetVertexBuffer, RHICommandType::SetConstants, RHICommandType::Draw };
    std::vector<RHICommandType> expected = { RHICommandType::BeginPass };
    expected.insert(expected.end(), draw.begin(), draw.end());
    expected.insert(expected.end(), draw.begin(), draw.end());
    expected.push_back(RHICommandType::EndPass);
    CHECK(fixture.rhi.last_submission() == expected);

    Renderer::end_frame();
    CHECK(fixture.rhi.last_submission().size() == 2);
}

TEST_CASE("draw_item_add_texture throws past DRAW_ITEM_MAX_TEXTURES")
{
    NullRHI rhi;
    DrawItem item;
    RHITexturePtr texture = rhi.create_texture({ .width = 1, .height = 1 });
    for (uint32_t i = 0; i < DRAW_ITEM_MAX_TEXTURES; ++i)
    {
        CHECK_NOTHROW(draw_item_add_texture(item, texture));
    }
    CHECK_THROWS_AS(draw_item_add_texture(item, texture), Error);
}

TEST_CASE("Renderer: indexed and textured items record their binds")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    DrawFixture fixture;
    GraphicsPipelineHandle quad = fixture.quad();

    DrawItem item;
    item.pipeline = quad;
    item.vertex_buffers[0] = fixture.rhi.create_buffer({ .size = 256, .usage = RHIBufferUsage::Vertex });
    item.index_buffer = fixture.rhi.create_buffer({ .size = 64, .usage = RHIBufferUsage::Index });
    item.index_count = 6;
    const float matrix[16] = {};
    draw_item_set_constants(item, matrix);
    for (uint32_t i = 0; i < QuadPS::DEFAULT_TEXTURES; ++i)
    {
        draw_item_add_texture(item, fixture.rhi.create_texture({ .width = 1, .height = 1 }));
    }
    item.sampler = fixture.rhi.create_sampler({});

    Renderer::submit(item);
    Renderer::end_frame();

    const std::vector<RHICommandType>& commands = fixture.rhi.last_submission();
    CHECK(commands.size() == 24);
    CHECK(commands[3] == RHICommandType::SetIndexBuffer);
    CHECK(commands[4] == RHICommandType::SetConstants);
    CHECK(commands[5] == RHICommandType::BindTexture);
    CHECK(commands[20] == RHICommandType::BindTexture);
    CHECK(commands[21] == RHICommandType::BindSampler);
    CHECK(commands[22] == RHICommandType::DrawIndexed);
}

TEST_CASE("Renderer: a bad draw item discards the frame and the next frame recovers")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    DrawFixture fixture;
    GraphicsPipelineHandle solid = fixture.solid();

    DrawItem item = fixture.solid_item(solid);
    item.sampler = fixture.rhi.create_sampler({});
    Renderer::submit(item);
    CHECK_THROWS_AS(Renderer::end_frame(), Error);
    CHECK(fixture.rhi.submit_count() == 0);

    Renderer::end_frame();
    CHECK(fixture.rhi.submit_count() == 1);

    Renderer::submit({});
    CHECK_THROWS_AS(Renderer::end_frame(), Error);
}

TEST_CASE("Renderer: shaders are compiled by init")
{
    RendererGuard guard;
    CHECK_THROWS_AS(Renderer::shaders(), Error);
    CHECK_THROWS_AS(Renderer::pipeline({}), Error);

    Renderer::init({ RHIBackend::Null });
    const ShaderLibrary& shaders = Renderer::shaders();
    CHECK(shaders.contains<SolidVS>());
    CHECK(shaders.contains<SolidPS>());
    CHECK(shaders.contains<QuadVS>());
    CHECK(shaders.contains<QuadPS>(0));
    CHECK(shaders.contains<QuadPS>(1));
    CHECK(shaders.contains<CircleVS>());
    CHECK(shaders.contains<CirclePS>());
}

TEST_CASE("Renderer: default resources")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    REQUIRE(Renderer::white_texture());
    CHECK(Renderer::white_texture()->width() == 1);
    CHECK(Renderer::white_texture()->height() == 1);
    CHECK(Renderer::default_sampler());
}

TEST_CASE("Renderer: back buffer format reaches the viewport")
{
    RendererGuard guard;
    Renderer::init({ .backend = RHIBackend::Null, .back_buffer_format = RHIFormat::RGBA8Unorm });
    CHECK(Renderer::back_buffer_format() == RHIFormat::RGBA8Unorm);
}

namespace
{

struct RecordFixture
{
    NullRHI& rhi;
    GraphicsPipelineCache cache;
    DefaultResources defaults;
    RHIViewportPtr viewport;
    RHIRenderTargetPtr target;
    RHICommandList commands;

    RecordFixture()
        : rhi(static_cast<NullRHI&>(Renderer::rhi()))
        , defaults(create_default_resources(rhi))
    {
        viewport = rhi.create_viewport({ .width = 8, .height = 8 });
        target = viewport->acquire_back_buffer();
        commands.begin_pass(target.get(), {});
    }

    ~RecordFixture() { commands.clear(); }

    DrawItem item(const GraphicsPipelineDesc& desc)
    {
        DrawItem result;
        result.pipeline = cache.get_or_create(rhi, desc);
        result.vertex_buffers[0] = rhi.create_buffer({ .size = 256, .usage = RHIBufferUsage::Vertex });
        result.vertex_count = 3;
        return result;
    }

    std::vector<const RHIBindTextureCommand*> texture_binds() const
    {
        std::vector<const RHIBindTextureCommand*> binds;
        for (const RHICommand& command : commands)
        {
            if (command.type() == RHICommandType::BindTexture)
            {
                binds.push_back(static_cast<const RHIBindTextureCommand*>(&command));
            }
        }
        return binds;
    }

    uint32_t count(RHICommandType type) const
    {
        uint32_t total = 0;
        for (const RHICommand& command : commands)
        {
            total += command.type() == type ? 1 : 0;
        }
        return total;
    }
};

} // namespace

TEST_CASE("record_draw_item fills unset texture slots with the white default")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    const GraphicsPipelineDesc quad = builtin_pipeline_desc(BuiltinPipeline::Quad, Renderer::shaders(), Renderer::back_buffer_format());

    DrawItem item = fixture.item(quad);
    RHITexturePtr own = fixture.rhi.create_texture({ .width = 2, .height = 2 });
    draw_item_add_texture(item, own);
    record_draw_item(fixture.commands, item, fixture.cache, fixture.defaults);

    const std::vector<const RHIBindTextureCommand*> binds = fixture.texture_binds();
    REQUIRE(binds.size() == 16);
    CHECK(&binds[0]->texture() == own.get());
    for (uint32_t i = 1; i < 16; ++i)
    {
        CHECK(binds[i]->array_index() == i);
        CHECK(&binds[i]->texture() == fixture.defaults.white_texture.get());
    }
    CHECK(fixture.count(RHICommandType::BindSampler) == 1);
}

TEST_CASE("record_draw_item fills every slot when the item has no textures")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    record_draw_item(fixture.commands, fixture.item(builtin_pipeline_desc(BuiltinPipeline::Quad, Renderer::shaders(), Renderer::back_buffer_format())), fixture.cache, fixture.defaults);
    CHECK(fixture.texture_binds().size() == 16);
}

TEST_CASE("record_draw_item sizes the fill from the pipeline's texture array")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    record_draw_item(fixture.commands, fixture.item(builtin_pipeline_desc(BuiltinPipeline::Quad, Renderer::shaders(), Renderer::back_buffer_format(), 1)), fixture.cache, fixture.defaults);
    CHECK(fixture.texture_binds().size() == 32);
}

TEST_CASE("record_draw_item leaves pipelines without texture bindings alone")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    record_draw_item(fixture.commands, fixture.item(builtin_pipeline_desc(BuiltinPipeline::SolidTriangles, Renderer::shaders(), Renderer::back_buffer_format())), fixture.cache, fixture.defaults);
    CHECK(fixture.count(RHICommandType::BindTexture) == 0);
    CHECK(fixture.count(RHICommandType::BindSampler) == 0);

    DrawItem textured = fixture.item(builtin_pipeline_desc(BuiltinPipeline::SolidTriangles, Renderer::shaders(), Renderer::back_buffer_format()));
    draw_item_add_texture(textured, fixture.defaults.white_texture);
    CHECK_THROWS_AS(record_draw_item(fixture.commands, textured, fixture.cache, fixture.defaults), Error);
}

TEST_CASE("record_draw_item binds the default sampler only when the item has none")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    const GraphicsPipelineDesc quad = builtin_pipeline_desc(BuiltinPipeline::Quad, Renderer::shaders(), Renderer::back_buffer_format());

    record_draw_item(fixture.commands, fixture.item(quad), fixture.cache, fixture.defaults);
    RHISamplerPtr own = fixture.rhi.create_sampler({});
    DrawItem explicit_sampler = fixture.item(quad);
    explicit_sampler.sampler = own;
    record_draw_item(fixture.commands, explicit_sampler, fixture.cache, fixture.defaults);

    std::vector<const RHIBindSamplerCommand*> binds;
    for (const RHICommand& command : fixture.commands)
    {
        if (command.type() == RHICommandType::BindSampler)
        {
            binds.push_back(static_cast<const RHIBindSamplerCommand*>(&command));
        }
    }
    REQUIRE(binds.size() == 2);
    CHECK(&binds[0]->sampler() == fixture.defaults.sampler.get());
    CHECK(&binds[1]->sampler() == own.get());
}

TEST_CASE("record_draw_item rejects more textures than the pipeline has slots")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    DrawItem item = fixture.item(builtin_pipeline_desc(BuiltinPipeline::Quad, Renderer::shaders(), Renderer::back_buffer_format(), 1));
    item.texture_count = DRAW_ITEM_MAX_TEXTURES;
    for (uint32_t i = 0; i < DRAW_ITEM_MAX_TEXTURES; ++i)
    {
        item.textures[i] = fixture.defaults.white_texture;
    }
    CHECK_NOTHROW(record_draw_item(fixture.commands, item, fixture.cache, fixture.defaults));

    DrawItem narrow = fixture.item(builtin_pipeline_desc(BuiltinPipeline::Quad, Renderer::shaders(), Renderer::back_buffer_format()));
    narrow.texture_count = QuadPS::DEFAULT_TEXTURES + 1;
    CHECK_THROWS_AS(record_draw_item(fixture.commands, narrow, fixture.cache, fixture.defaults), Error);

    DrawItem circle = fixture.item(builtin_pipeline_desc(BuiltinPipeline::Circle, Renderer::shaders(), Renderer::back_buffer_format()));
    draw_item_add_texture(circle, fixture.defaults.white_texture);
    CHECK_THROWS_AS(record_draw_item(fixture.commands, circle, fixture.cache, fixture.defaults), Error);
}

TEST_CASE("Renderer: queue capacity survives frames")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    DrawFixture fixture;
    const GraphicsPipelineHandle solid = fixture.solid();
    for (uint32_t frame = 0; frame < 3; ++frame)
    {
        for (uint32_t i = 0; i < 100; ++i)
        {
            Renderer::submit(fixture.solid_item(solid));
        }
        Renderer::end_frame();
        CHECK(fixture.rhi.last_submission().size() == 2 + 100 * 4);
    }
}

TEST_CASE("Renderer: releasing pipelines stales handles and the next frame recovers")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    DrawFixture fixture;
    const GraphicsPipelineHandle solid = fixture.solid();
    CHECK_NOTHROW(Renderer::resolve_pipeline(solid));

    Renderer::submit(fixture.solid_item(solid));
    Renderer::release_pipelines();
    CHECK(Renderer::pipeline_cache_stats().entries == 0);
    CHECK_THROWS_AS(Renderer::resolve_pipeline(solid), Error);
    CHECK_THROWS_AS(Renderer::end_frame(), Error);

    const GraphicsPipelineHandle again = fixture.solid();
    CHECK(again != solid);
    CHECK_NOTHROW(Renderer::resolve_pipeline(again));
    Renderer::submit(fixture.solid_item(again));
    Renderer::end_frame();
    CHECK(fixture.rhi.last_submission().size() == 6);
}

TEST_CASE("Renderer: trim keeps the renderer usable")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    DrawFixture fixture;
    const std::vector<RHICommandType> before = [&]
    {
        Renderer::submit(fixture.solid_item(fixture.solid()));
        Renderer::end_frame();
        return fixture.rhi.last_submission();
    }();

    Renderer::trim();
    CHECK(Renderer::pipeline_cache_stats().entries == 0);
    CHECK(Renderer::shaders().contains<SolidVS>());
    CHECK(Renderer::white_texture());

    Renderer::submit(fixture.solid_item(fixture.solid()));
    Renderer::end_frame();
    CHECK(fixture.rhi.last_submission() == before);

    Renderer::release_shader_cache();
    CHECK(Renderer::shaders().contains<QuadPS>(1));
}

TEST_CASE("Renderer: a draw item binds every vertex stream it fills")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    DrawFixture fixture;

    DrawItem item = fixture.solid_item(fixture.solid());
    item.vertex_buffers[2] = fixture.rhi.create_buffer({ .size = 256, .usage = RHIBufferUsage::Vertex });
    item.vertex_offsets[2] = 16;
    Renderer::submit(item);
    Renderer::end_frame();

    uint32_t vertex_binds = 0;
    for (const RHICommandType type : fixture.rhi.last_submission())
    {
        vertex_binds += type == RHICommandType::SetVertexBuffer ? 1 : 0;
    }
    CHECK(vertex_binds == 2);

    VertexBuffer buffer = Renderer::create_vertex_buffer(vertex_declaration<Vertex2DLine>(), 4, BufferMode::Static);
    DrawItem staged;
    CHECK_NOTHROW(draw_item_set_vertex_stream(staged, buffer, 0, RHI_MAX_VERTEX_SLOTS - 1));
    CHECK_THROWS_AS(draw_item_set_vertex_stream(staged, buffer, 0, RHI_MAX_VERTEX_SLOTS), Error);
}
