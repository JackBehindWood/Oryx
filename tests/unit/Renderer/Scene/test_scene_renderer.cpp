#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "unit/MemoryTestSupport.h"
#include "unit/Renderer/RenderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct RendererGuard
{
    RendererGuard() { Renderer::init({ RHIBackend::Null }); }
    ~RendererGuard() { Renderer::shutdown(); }
};

const Colour WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };

// Records every (source, stage) it is asked to draw in and draws one rect in the 2D stages.
class RecordingSource final : public RenderSource
{
public:
    RecordingSource(std::string name, std::vector<std::string>& log)
        : m_name(std::move(name))
        , m_log(log)
    {
    }

    void render_stage(RenderStage stage, StageContext& context) override
    {
        m_log.push_back(m_name + ":" + render_stage_name(stage));
        if (stage == RenderStage::Scene2D || stage == RenderStage::Overlay)
        {
            CHECK(context.batcher_2d.open());
            context.batcher_2d.draw_rect({ 1.0f, 1.0f }, { 1.0f, 1.0f }, WHITE);
        }
        else
        {
            CHECK_FALSE(context.batcher_2d.open());
        }
    }

private:
    std::string m_name;
    std::vector<std::string>& m_log;
};

} // namespace

TEST_CASE("SceneRenderer: a scene opens once, needs a scene to submit, and must end before the frame")
{
    RendererGuard guard;
    Camera2D camera(10.0f, 10.0f);
    SceneRenderer& scene = Renderer::scene();
    std::vector<std::string> log;
    RecordingSource source("a", log);

    CHECK_THROWS_AS(scene.submit(source), Error);
    CHECK_THROWS_AS(scene.end_scene(), Error);
    scene.begin_scene(view_over(camera));
    CHECK(scene.open());
    CHECK_THROWS_AS(scene.begin_scene(view_over(camera)), Error);
    CHECK_THROWS_AS(Renderer::end_frame(), Error);
    scene.end_scene();
    CHECK_FALSE(scene.open());
    CHECK_NOTHROW(Renderer::end_frame());
}

TEST_CASE("SceneRenderer runs each stage across every source before the next stage")
{
    RendererGuard guard;
    Camera2D camera(10.0f, 10.0f);
    std::vector<std::string> log;
    RecordingSource first("a", log);
    RecordingSource second("b", log);

    SceneRenderer& scene = Renderer::scene();
    scene.begin_scene(view_over(camera));
    scene.submit(first);
    scene.submit(second);
    scene.end_scene();

    const std::vector<std::string> expected = { "a:Opaque3D", "b:Opaque3D", "a:Transparent3D", "b:Transparent3D", "a:Scene2D", "b:Scene2D", "a:Overlay", "b:Overlay" };
    CHECK(log == expected);
    CHECK_FALSE(Renderer::scene().batcher_2d().open());
    CHECK(Renderer::batch_stats().primitives == 4);
}

TEST_CASE("SceneRenderer draws Scene2D under the view camera and Overlay in screen space")
{
    RendererGuard guard;
    Camera2D camera(10.0f, 10.0f);
    camera.set_zoom(2.0f);
    std::vector<std::string> log;
    RecordingSource source("a", log);
    Renderer::scene().render(view_over(camera, 320.0f, 200.0f), source);

    const std::vector<DrawItem>& items = Renderer::scene().pass(RENDER_PASS_MAIN).items;
    REQUIRE(items.size() == 2);
    float expected_world[16];
    float expected_screen[16];
    camera.to_gpu(expected_world);
    Camera2D::screen_space(320.0f, 200.0f).to_gpu(expected_screen);
    CHECK(std::memcmp(items[0].constants, expected_world, sizeof(expected_world)) == 0);
    CHECK(std::memcmp(items[1].constants, expected_screen, sizeof(expected_screen)) == 0);
    Renderer::end_frame();
}

TEST_CASE("SceneRenderer opens nothing for a scene with no source and no debug shape")
{
    RendererGuard guard;
    Camera2D camera(10.0f, 10.0f);
    SceneRenderer& scene = Renderer::scene();
    scene.begin_scene(view_over(camera));
    scene.end_scene();
    CHECK(Renderer::batch_stats().draws == 0);

    RenderSource nothing;
    scene.render(view_over(camera), nothing);
    CHECK(Renderer::batch_stats().draws == 0);
    CHECK_FALSE(scene.batcher_2d().open());
}

TEST_CASE("SceneRenderer closes the scene when a stage throws and can be used again")
{
    RendererGuard guard;
    Camera2D camera(10.0f, 10.0f);
    FnSource throwing([](RenderStage stage, StageContext& context)
    {
        if (stage == RenderStage::Scene2D)
        {
            context.batcher_2d.draw_rect({ 1.0f, 1.0f }, { 1.0f, 1.0f }, WHITE);
            throw Error("draw failed");
        }
    });
    SceneRenderer& scene = Renderer::scene();
    CHECK_THROWS_AS(scene.render(view_over(camera), throwing), Error);
    CHECK_FALSE(scene.open());
    CHECK_FALSE(scene.batcher_2d().open());

    std::vector<std::string> log;
    RecordingSource fine("a", log);
    CHECK_NOTHROW(scene.render(view_over(camera), fine));
    scene.clear_items();
    CHECK_NOTHROW(Renderer::end_frame());
}

TEST_CASE("SceneScope ends the scene on exit and on unwind")
{
    RendererGuard guard;
    Camera2D camera(10.0f, 10.0f);
    SceneRenderer& scene = Renderer::scene();
    {
        SceneScope scope(scene, view_over(camera));
        CHECK(scene.open());
    }
    CHECK_FALSE(scene.open());

    std::vector<std::string> log;
    RecordingSource source("a", log);
    CHECK_THROWS_WITH_AS(([&] {
        SceneScope scope(scene, view_over(camera));
        scene.submit(source);
        throw Error("client failed");
    }()), "client failed", Error);
    CHECK_FALSE(scene.open());
    CHECK_FALSE(scene.batcher_2d().open());
    CHECK_THROWS_AS(([&] {
        SceneScope scope(scene, view_over(camera));
        scene.end_scene();
    }()), Error);
}

TEST_CASE("SceneRenderer keeps direct submissions and emitted items in order beside batched draws")
{
    RendererGuard guard;
    RHIViewportPtr viewport = static_cast<NullRHI&>(Renderer::rhi()).create_viewport({ .width = 8, .height = 8 });
    Renderer::set_viewport(viewport);
    Camera2D camera(10.0f, 10.0f);

    auto marker = [](uint32_t count)
    {
        DrawItem item;
        item.pipeline = Renderer::pipeline(pipeline_def(Primitive2D::Triangle));
        item.vertex_buffers[0] = Renderer::rhi().create_buffer({ .size = 256, .usage = RHIBufferUsage::Vertex });
        item.vertex_count = count;
        return item;
    };
    Renderer::submit(marker(3));
    FnSource source([&](RenderStage stage, StageContext& context)
    {
        if (stage == RenderStage::Opaque3D)
        {
            context.emit(marker(6));
        }
        else if (stage == RenderStage::Scene2D)
        {
            context.batcher_2d.draw_triangle({ 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, WHITE);
        }
    });
    Renderer::scene().render(view_over(camera), source);

    const std::vector<DrawItem>& items = Renderer::scene().pass(RENDER_PASS_MAIN).items;
    REQUIRE(items.size() == 3);
    CHECK(items[0].vertex_count == 3);
    CHECK(items[1].vertex_count == 6);
    CHECK(items[2].vertex_count == 3);
    CHECK_NOTHROW(Renderer::end_frame());
    Renderer::set_viewport({});
}

TEST_CASE("SceneRenderer submits and ends allocation-free once warm")
{
    RendererGuard guard;
    Camera2D camera(10.0f, 10.0f);
    RenderSource nothing;
    SceneRenderer& scene = Renderer::scene();
    const RenderView view = view_over(camera);

    for (int32_t frame = 0; frame < 2; ++frame)
    {
        scene.begin_scene(view);
        scene.submit(nothing);
        scene.submit(nothing);
        scene.end_scene();
    }
    MemoryStats before = all_allocations();
    scene.begin_scene(view);
    scene.submit(nothing);
    scene.submit(nothing);
    scene.end_scene();
    MemoryStats delta = memory_delta(before, all_allocations());
    CHECK(delta.allocation_count == 0);
}

TEST_CASE("SceneRenderer resolves a clip against the framebuffer scale and records scissor only on change")
{
    RendererGuard guard;
    NullRHI& rhi = static_cast<NullRHI&>(Renderer::rhi());
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 200, .height = 100 });
    Renderer::set_viewport(viewport);
    const Camera2D camera = Camera2D::screen_space(100.0f, 50.0f);
    const RenderView view{ camera, { 100.0f, 50.0f }, { 200.0f, 100.0f }, 2.0f };
    FnSource source([](RenderStage stage, StageContext& context)
    {
        if (stage != RenderStage::Scene2D)
        {
            return;
        }
        BatchRenderer2D& batcher = context.batcher_2d;
        batcher.draw_rect({ 5.0f, 5.0f }, { 2.0f, 2.0f }, WHITE);
        {
            ClipScope clip(batcher, { 25.0f, 10.0f }, { 50.0f, 20.0f });
            batcher.draw_rect({ 5.0f, 5.0f }, { 2.0f, 2.0f }, WHITE);
        }
        batcher.draw_rect({ 5.0f, 5.0f }, { 2.0f, 2.0f }, WHITE);
    });
    Renderer::scene().render(view, source);

    const std::vector<DrawItem>& items = Renderer::scene().pass(RENDER_PASS_MAIN).items;
    REQUIRE(items.size() == 3);
    REQUIRE(items[1].has_scissor);
    CHECK(items[1].scissor.y == 60);
    CHECK(items[1].scissor.width == 100);
    CHECK(items[1].scissor.height == 40);

    CHECK(Renderer::end_frame());
    const std::vector<std::string_view> commands = rhi.last_submission();
    CHECK(std::count(commands.begin(), commands.end(), std::string_view("SetScissor")) == 2);
    CHECK(std::count(commands.begin(), commands.end(), std::string_view("Draw")) + std::count(commands.begin(), commands.end(), std::string_view("DrawIndexed")) == 3);

    Renderer::scene().render(view, source);
    CHECK(Renderer::end_frame());
    Renderer::set_viewport({});
}

namespace
{

// Draws one rect in Scene2D and keeps the view and camera it was handed.
class ViewProbe final : public RenderSource
{
public:
    explicit ViewProbe(std::vector<std::string>* log = nullptr, std::string name = {})
        : m_log(log)
        , m_name(std::move(name))
    {
    }

    void render_stage(RenderStage stage, StageContext& context) override
    {
        if (stage != RenderStage::Scene2D && stage != RenderStage::Overlay)
        {
            return;
        }
        if (m_log != nullptr)
        {
            m_log->push_back(m_name + ":" + render_stage_name(stage));
        }
        if (stage == RenderStage::Scene2D)
        {
            logical = context.view.logical;
            framebuffer = context.view.framebuffer;
            camera = &context.view.camera;
            const Camera2D& camera_2d = static_cast<const Camera2D&>(context.view.camera);
            origin = camera_2d.world_to_screen({ 0.0f, 0.0f });
            far_corner = camera_2d.world_to_screen(context.view.logical);
        }
        context.batcher_2d.draw_rect({ 1.0f, 1.0f }, { 1.0f, 1.0f }, WHITE);
    }

    Vec2f logical;
    Vec2f framebuffer;
    Vec2f origin;
    Vec2f far_corner;
    const Camera* camera = nullptr;

private:
    std::vector<std::string>* m_log;
    std::string m_name;
};

RenderView surface_view(const Camera& camera, float scale = 1.0f)
{
    return { camera, { 400.0f, 300.0f }, { 400.0f * scale, 300.0f * scale }, scale };
}

} // namespace

TEST_CASE("SceneRenderer: a region source sees a region-sized view whose origin lands on the region corner and is clipped to it")
{
    RendererGuard guard;
    const Camera2D camera = Camera2D::screen_space(400.0f, 300.0f);
    ViewProbe probe;
    SceneRenderer& scene = Renderer::scene();
    scene.begin_scene(surface_view(camera));
    {
        SubmitScope region(scene, { { { 100.0f, 50.0f }, { 200.0f, 100.0f } }, true, k_layer_world });
        scene.submit(probe);
    }
    scene.end_scene();

    CHECK(probe.logical == Vec2f(200.0f, 100.0f));
    CHECK(probe.framebuffer == Vec2f(200.0f, 100.0f));
    CHECK(probe.origin[0] == doctest::Approx(100.0f));
    CHECK(probe.origin[1] == doctest::Approx(150.0f));
    CHECK(probe.far_corner[0] == doctest::Approx(300.0f));
    CHECK(probe.far_corner[1] == doctest::Approx(50.0f));
    const std::vector<DrawItem>& items = scene.pass(RENDER_PASS_MAIN).items;
    REQUIRE(items.size() == 2);
    for (const DrawItem& item : items)
    {
        REQUIRE(item.has_scissor);
        CHECK(item.scissor.x == 100);
        CHECK(item.scissor.y == 50);
        CHECK(item.scissor.width == 200);
        CHECK(item.scissor.height == 100);
    }
}

TEST_CASE("SceneRenderer: a region keeps its place at a HiDPI scale")
{
    RendererGuard guard;
    const Camera2D camera = Camera2D::screen_space(400.0f, 300.0f);
    ViewProbe probe;
    SceneRenderer& scene = Renderer::scene();
    scene.begin_scene(surface_view(camera, 2.0f));
    {
        SubmitScope region(scene, { { { 100.0f, 50.0f }, { 200.0f, 100.0f } }, true, k_layer_world });
        scene.submit(probe);
    }
    scene.end_scene();

    CHECK(probe.framebuffer == Vec2f(400.0f, 200.0f));
    const std::vector<DrawItem>& items = scene.pass(RENDER_PASS_MAIN).items;
    REQUIRE(items.size() == 2);
    for (const DrawItem& item : items)
    {
        REQUIRE(item.has_scissor);
        CHECK(item.scissor.x == 200);
        CHECK(item.scissor.y == 100);
        CHECK(item.scissor.width == 400);
        CHECK(item.scissor.height == 200);
    }
}

TEST_CASE("SceneRenderer: a region covering the surface draws exactly like no region")
{
    RendererGuard guard;
    const Camera2D camera = Camera2D::screen_space(400.0f, 300.0f);
    ViewProbe plain;
    ViewProbe covering;
    SceneRenderer& scene = Renderer::scene();

    scene.begin_scene(surface_view(camera));
    scene.submit(plain);
    scene.end_scene();
    const std::vector<DrawItem> expected = scene.pass(RENDER_PASS_MAIN).items;
    scene.clear_items();

    scene.begin_scene(surface_view(camera));
    {
        SubmitScope region(scene, { { { 0.0f, 0.0f }, { 400.0f, 300.0f } }, true, k_layer_world });
        scene.submit(covering);
    }
    scene.end_scene();
    const std::vector<DrawItem>& actual = scene.pass(RENDER_PASS_MAIN).items;

    REQUIRE(actual.size() == expected.size());
    CHECK_FALSE(actual[0].has_scissor);
    CHECK(covering.camera == &camera);
    CHECK(covering.logical == plain.logical);
    CHECK(std::memcmp(actual[0].constants, expected[0].constants, actual[0].constants_size) == 0);
    CHECK(actual[0].vertex_count == expected[0].vertex_count);
}

TEST_CASE("SceneRenderer: each region source binds its own camera and the stages stay stage-major")
{
    RendererGuard guard;
    const Camera2D camera = Camera2D::screen_space(400.0f, 300.0f);
    std::vector<std::string> log;
    ViewProbe left(&log, "left");
    ViewProbe right(&log, "right");
    SceneRenderer& scene = Renderer::scene();
    scene.begin_scene(surface_view(camera));
    {
        SubmitScope region(scene, { { { 0.0f, 0.0f }, { 150.0f, 300.0f } }, true, k_layer_world });
        scene.submit(left);
    }
    {
        SubmitScope region(scene, { { { 200.0f, 0.0f }, { 200.0f, 300.0f } }, true, k_layer_world });
        scene.submit(right);
    }
    scene.end_scene();

    const std::vector<std::string> expected = { "left:Scene2D", "right:Scene2D", "left:Overlay", "right:Overlay" };
    CHECK(log == expected);
    CHECK(left.origin[0] == doctest::Approx(0.0f));
    CHECK(right.origin[0] == doctest::Approx(200.0f));
    const std::vector<DrawItem>& items = scene.pass(RENDER_PASS_MAIN).items;
    REQUIRE(items.size() == 4);
    CHECK(std::memcmp(items[0].constants, items[1].constants, items[0].constants_size) != 0);
}

TEST_CASE("SceneRenderer orders sources by layer, then by submission")
{
    RendererGuard guard;
    const Camera2D camera = Camera2D::screen_space(400.0f, 300.0f);
    std::vector<std::string> log;
    ViewProbe interface_first(&log, "interface_first");
    ViewProbe world_a(&log, "world_a");
    ViewProbe world_b(&log, "world_b");
    ViewProbe interface_second(&log, "interface_second");
    SceneRenderer& scene = Renderer::scene();
    scene.begin_scene(surface_view(camera));
    {
        SubmitScope layer(scene, { {}, false, k_layer_interface });
        scene.submit(interface_first);
    }
    scene.submit(world_a);
    scene.submit(world_b);
    {
        SubmitScope layer(scene, { {}, false, k_layer_interface });
        scene.submit(interface_second);
    }
    scene.end_scene();

    const std::vector<std::string> expected = { "world_a:Scene2D", "world_b:Scene2D", "interface_first:Scene2D", "interface_second:Scene2D",
                                                "world_a:Overlay", "world_b:Overlay", "interface_first:Overlay", "interface_second:Overlay" };
    CHECK(log == expected);
}

TEST_CASE("SubmitScope restores the previous submit context and a new scene starts from the default")
{
    RendererGuard guard;
    const Camera2D camera = Camera2D::screen_space(400.0f, 300.0f);
    SceneRenderer& scene = Renderer::scene();
    scene.begin_scene(surface_view(camera));
    {
        SubmitScope outer(scene, { {}, false, k_layer_interface });
        {
            SubmitScope inner(scene, { { { 1.0f, 1.0f }, { 2.0f, 2.0f } }, true, 5 });
            CHECK(scene.submit_context().layer == 5);
        }
        CHECK(scene.submit_context().layer == k_layer_interface);
        scene.set_submit_context({ {}, false, 7 });
    }
    CHECK(scene.submit_context().layer == k_layer_world);
    scene.set_submit_context({ {}, false, 9 });
    scene.end_scene();
    scene.begin_scene(surface_view(camera));
    CHECK(scene.submit_context().layer == k_layer_world);
    scene.end_scene();
}
