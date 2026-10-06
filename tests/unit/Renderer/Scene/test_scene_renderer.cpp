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
