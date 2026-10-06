#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "unit/Renderer/RenderTestSupport.h"
#include "Oryx/Renderer/FrameRecorder.h"

using namespace oryx;

namespace
{

std::vector<std::string_view> without_debug_groups(const std::vector<std::string_view>& submission)
{
    std::vector<std::string_view> filtered;
    for (const std::string_view name : submission)
    {
        if (name != "PushDebugGroup" && name != "PopDebugGroup")
        {
            filtered.push_back(name);
        }
    }
    return filtered;
}

} // namespace

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

    GraphicsPipelineHandle solid() { return Renderer::pipeline(pipeline_desc(pipeline_def(Primitive2D::Triangle), Renderer::shaders(), Renderer::back_buffer_format())); }
    GraphicsPipelineHandle quad() { return Renderer::pipeline(pipeline_desc(pipeline_def(Primitive2D::Quad), Renderer::shaders(), Renderer::back_buffer_format())); }

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
    CHECK(without_debug_groups(rhi.last_submission()) == std::vector<std::string_view>{ "BeginPass", "EndPass" });

    viewport.reset();
    Renderer::set_viewport({});
    Renderer::shutdown();
    CHECK_FALSE(Renderer::initialised());
    Renderer::shutdown();
}

TEST_CASE("Renderer: a presented frame is bracketed by balanced Frame and Main pass debug groups")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    NullRHI& rhi = static_cast<NullRHI&>(Renderer::rhi());
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 8, .height = 8 });
    Renderer::set_viewport(viewport);
    CHECK(Renderer::end_frame());
    CHECK(rhi.last_submission() == std::vector<std::string_view>{ "PushDebugGroup", "BeginPass", "PushDebugGroup", "PopDebugGroup", "EndPass", "PopDebugGroup" });
    viewport.reset();
    Renderer::set_viewport({});
}

TEST_CASE("Renderer: zero-size viewport drops the frame")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    NullRHI& rhi = static_cast<NullRHI&>(Renderer::rhi());
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 0, .height = 0 });
    Renderer::set_viewport(viewport);
    CHECK_FALSE(Renderer::end_frame());
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

    const std::vector<std::string_view> draw = { "SetPipeline", "SetVertexBuffer", "SetConstants", "Draw" };
    std::vector<std::string_view> expected = { "BeginPass" };
    expected.insert(expected.end(), draw.begin(), draw.end());
    expected.insert(expected.end(), draw.begin(), draw.end());
    expected.push_back("EndPass");
    CHECK(without_debug_groups(fixture.rhi.last_submission()) == expected);

    Renderer::end_frame();
    CHECK(without_debug_groups(fixture.rhi.last_submission()).size() == 2);
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
    for (uint32_t i = 0; i < TextureArrayPermutations::DEFAULT_TEXTURES; ++i)
    {
        draw_item_add_texture(item, fixture.rhi.create_texture({ .width = 1, .height = 1 }));
    }
    item.sampler = fixture.rhi.create_sampler({});

    Renderer::submit(item);
    Renderer::end_frame();

    const std::vector<std::string_view>& commands = without_debug_groups(fixture.rhi.last_submission());
    CHECK(commands.size() == 24);
    CHECK(commands[3] == "SetIndexBuffer");
    CHECK(commands[4] == "SetConstants");
    CHECK(commands[5] == "BindTexture");
    CHECK(commands[20] == "BindTexture");
    CHECK(commands[21] == "BindSampler");
    CHECK(commands[22] == "DrawIndexed");
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
    try
    {
        Renderer::end_frame();
        FAIL("end_frame did not throw");
    }
    catch (const Error& error)
    {
        CHECK(std::string(error.what()).find("Draw Batch 0") != std::string::npos);
    }
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
    CHECK_THROWS_AS(Renderer::pipeline(GraphicsPipelineDesc{}), Error);

    Renderer::init({ RHIBackend::Null });
    const ShaderLibrary& shaders = Renderer::shaders();
    CHECK(shaders.contains<SolidVS>());
    CHECK(shaders.contains<SolidPS>());
    CHECK(shaders.contains<QuadVS>());
    CHECK(shaders.contains<QuadPS>(0));
    CHECK(shaders.contains<QuadPS>(1));
    CHECK(shaders.contains<CircleVS>());
    CHECK(shaders.contains<CirclePS>());
    CHECK(shaders.contains<TextVS>());
    CHECK(shaders.contains<TextPS>(0));
    CHECK(shaders.contains<TextPS>(1));
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
            if (std::string_view(command.command_name()) == "BindTexture")
            {
                binds.push_back(static_cast<const RHIBindTextureCommand*>(&command));
            }
        }
        return binds;
    }

    uint32_t count(std::string_view name) const
    {
        uint32_t total = 0;
        for (const RHICommand& command : commands)
        {
            total += std::string_view(command.command_name()) == name ? 1 : 0;
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
    const GraphicsPipelineDesc quad = pipeline_desc(pipeline_def(Primitive2D::Quad), Renderer::shaders(), Renderer::back_buffer_format());

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
    CHECK(fixture.count("BindSampler") == 1);
}

TEST_CASE("record_draw_item fills every slot when the item has no textures")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    record_draw_item(fixture.commands, fixture.item(pipeline_desc(pipeline_def(Primitive2D::Quad), Renderer::shaders(), Renderer::back_buffer_format())), fixture.cache, fixture.defaults);
    CHECK(fixture.texture_binds().size() == 16);
}

TEST_CASE("record_draw_item sizes the fill from the pipeline's texture array")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    record_draw_item(fixture.commands, fixture.item(pipeline_desc(pipeline_def(Primitive2D::Quad), Renderer::shaders(), Renderer::back_buffer_format(), 1)), fixture.cache, fixture.defaults);
    CHECK(fixture.texture_binds().size() == 32);
}

TEST_CASE("record_draw_item leaves pipelines without texture bindings alone")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    record_draw_item(fixture.commands, fixture.item(pipeline_desc(pipeline_def(Primitive2D::Triangle), Renderer::shaders(), Renderer::back_buffer_format())), fixture.cache, fixture.defaults);
    CHECK(fixture.count("BindTexture") == 0);
    CHECK(fixture.count("BindSampler") == 0);

    DrawItem textured = fixture.item(pipeline_desc(pipeline_def(Primitive2D::Triangle), Renderer::shaders(), Renderer::back_buffer_format()));
    draw_item_add_texture(textured, fixture.defaults.white_texture);
    CHECK_THROWS_AS(record_draw_item(fixture.commands, textured, fixture.cache, fixture.defaults), Error);
}

TEST_CASE("record_draw_item binds the default sampler only when the item has none")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    const GraphicsPipelineDesc quad = pipeline_desc(pipeline_def(Primitive2D::Quad), Renderer::shaders(), Renderer::back_buffer_format());

    record_draw_item(fixture.commands, fixture.item(quad), fixture.cache, fixture.defaults);
    RHISamplerPtr own = fixture.rhi.create_sampler({});
    DrawItem explicit_sampler = fixture.item(quad);
    explicit_sampler.sampler = own;
    record_draw_item(fixture.commands, explicit_sampler, fixture.cache, fixture.defaults);

    std::vector<const RHIBindSamplerCommand*> binds;
    for (const RHICommand& command : fixture.commands)
    {
        if (std::string_view(command.command_name()) == "BindSampler")
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
    DrawItem item = fixture.item(pipeline_desc(pipeline_def(Primitive2D::Quad), Renderer::shaders(), Renderer::back_buffer_format(), 1));
    item.texture_count = DRAW_ITEM_MAX_TEXTURES;
    for (uint32_t i = 0; i < DRAW_ITEM_MAX_TEXTURES; ++i)
    {
        item.textures[i] = fixture.defaults.white_texture;
    }
    CHECK_NOTHROW(record_draw_item(fixture.commands, item, fixture.cache, fixture.defaults));

    DrawItem narrow = fixture.item(pipeline_desc(pipeline_def(Primitive2D::Quad), Renderer::shaders(), Renderer::back_buffer_format()));
    narrow.texture_count = TextureArrayPermutations::DEFAULT_TEXTURES + 1;
    CHECK_THROWS_AS(record_draw_item(fixture.commands, narrow, fixture.cache, fixture.defaults), Error);

    DrawItem circle = fixture.item(pipeline_desc(pipeline_def(Primitive2D::Circle), Renderer::shaders(), Renderer::back_buffer_format()));
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
        CHECK(without_debug_groups(fixture.rhi.last_submission()).size() == 2 + 100 * 4);
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
    CHECK(without_debug_groups(fixture.rhi.last_submission()).size() == 6);
}

TEST_CASE("Renderer: trim keeps the renderer usable")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    DrawFixture fixture;
    const std::vector<std::string_view> before = [&]
    {
        Renderer::submit(fixture.solid_item(fixture.solid()));
        Renderer::end_frame();
        return without_debug_groups(fixture.rhi.last_submission());
    }();

    Renderer::trim();
    CHECK(Renderer::pipeline_cache_stats().entries == 0);
    CHECK(Renderer::shaders().contains<SolidVS>());
    CHECK(Renderer::white_texture());

    Renderer::submit(fixture.solid_item(fixture.solid()));
    Renderer::end_frame();
    CHECK(without_debug_groups(fixture.rhi.last_submission()) == before);

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
    for (const std::string_view type : without_debug_groups(fixture.rhi.last_submission()))
    {
        vertex_binds += type == "SetVertexBuffer" ? 1 : 0;
    }
    CHECK(vertex_binds == 2);

    VertexBuffer buffer = VertexBuffer::create(Renderer::rhi(), vertex_declaration<Vertex2DLine>(), 4, BufferMode::Static);
    DrawItem staged;
    CHECK_NOTHROW(draw_item_set_vertex_stream(staged, buffer, 0, RHI_MAX_VERTEX_SLOTS - 1));
    CHECK_THROWS_AS(draw_item_set_vertex_stream(staged, buffer, 0, RHI_MAX_VERTEX_SLOTS), Error);
}

TEST_CASE("Renderer: a scene's Scene2D stage records one pass of batched draws")
{
    RendererGuard guard;
    Camera2D camera(2.0f, 2.0f);
    CHECK_THROWS_AS(Renderer::scene(), Error);
    Renderer::init({ RHIBackend::Null });
    DrawFixture fixture;
    const Colour red = { 1.0f, 0.0f, 0.0f, 1.0f };

    CHECK_THROWS_AS(Renderer::scene().batcher_2d().draw_rect({}, { 1.0f, 1.0f }, red), Error);
    CHECK_THROWS_AS(Renderer::scene().end_scene(), Error);

    test::render_2d(camera, [&](BatchRenderer2D& batcher)
    {
        batcher.draw_rect({ 0.0f, 0.0f }, { 1.0f, 1.0f }, red);
        batcher.draw_rect({ 0.5f, 0.0f }, { 1.0f, 1.0f }, red);
        batcher.draw_circle({ 0.0f, 0.0f }, 0.5f, red);
        batcher.draw_line({ -1.0f, 0.0f }, { 1.0f, 0.0f }, red);
        batcher.draw_triangle({ 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, red);
    });
    CHECK(Renderer::batch_stats().draws == 4);

    Renderer::end_frame();
    const std::vector<std::string_view> submission = without_debug_groups(fixture.rhi.last_submission());
    CHECK(submission.front() == "BeginPass");
    CHECK(submission.back() == "EndPass");
    CHECK(std::count(submission.begin(), submission.end(), "DrawIndexed") == 2);
    CHECK(std::count(submission.begin(), submission.end(), "Draw") == 2);
    CHECK(Renderer::batch_stats().draws == 0);

    BatchRenderer2D& batcher = Renderer::scene().batcher_2d();
    batcher.begin(camera);
    batcher.draw_rect({}, { 1.0f, 1.0f }, red);
    batcher.flush();
    CHECK(Renderer::batch_stats().draws == 1);
    CHECK_THROWS_AS(Renderer::end_frame(), Error);
    batcher.end();
    Renderer::end_frame();

    Renderer::scene().begin_scene(test::view_over(camera));
    CHECK_THROWS_AS(Renderer::end_frame(), Error);
    Renderer::scene().end_scene();
    Renderer::end_frame();
}

TEST_CASE("record_draw_item draws textured items through a fallback pixel shader")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    RecordFixture fixture;
    GraphicsPipelineDesc quad = pipeline_desc(pipeline_def(Primitive2D::Quad), Renderer::shaders(), Renderer::back_buffer_format());
    CHECK_FALSE(quad.shaders.pixel->is_fallback());

    ShaderType broken;
    for (const ShaderType& type : registered_shader_types())
    {
        if (type.type == std::type_index(typeid(QuadPS)))
        {
            broken = type;
        }
    }
    broken.source = "/Missing/Quad.slang";
    ShaderLibrary library;
    library.set_error_fallback(true);
    ShaderCache cache;
    library.compile(fixture.rhi, cache, broken);
    quad.shaders.pixel = library.get<QuadPS>();
    REQUIRE(quad.shaders.pixel->is_fallback());

    DrawItem item = fixture.item(quad);
    draw_item_add_texture(item, fixture.defaults.white_texture);
    CHECK(fixture.cache.resolve(item.pipeline).is_fallback());
    CHECK_NOTHROW(record_draw_item(fixture.commands, item, fixture.cache, fixture.defaults));
    CHECK(fixture.count("BindTexture") == 0);
    CHECK(fixture.count("Draw") == 1);
}
