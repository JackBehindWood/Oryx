#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "unit/Renderer/RenderTestSupport.h"
#include "Oryx/Renderer/RendererContext.h"
#include "../FakeFontSource.h"

using namespace oryx;

namespace
{

const Colour RED = { 1.0f, 0.0f, 0.0f, 1.0f };

struct RendererGuard
{
    ~RendererGuard() { Renderer::shutdown(); }
};

struct DebugFixture
{
    UniquePtr<RendererContext> context = create_renderer_context({ RHIBackend::Null });
    std::vector<DrawItem> sink;
    Camera2D camera{ 100.0f, 100.0f };
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);

    BatchRendererDesc desc() { return batch_renderer_desc(*context, sink); }

    ~DebugFixture() { context->rhi->wait_idle(); }
};

} // namespace

TEST_CASE("DebugRenderer: records with no device, scene or camera")
{
    DebugRenderer debug;
    debug.line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    debug.rect({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED, 0.5f);
    debug.circle({ 0.0f, 0.0f }, 2.0f, RED);
    CHECK(debug.size() == 3);
    debug.clear();
    CHECK(debug.size() == 0);
}

TEST_CASE("DebugRenderer: disabled records nothing and keeps what it already had")
{
    DebugRenderer debug;
    debug.line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    debug.set_enabled(false);
    CHECK_FALSE(debug.enabled());
    debug.line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    CHECK(debug.size() == 1);
}

TEST_CASE("DebugRenderer: shapes live for their frame count")
{
    DebugRenderer debug;
    debug.line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED, 3);
    debug.line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    debug.line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED, 0);
    CHECK(debug.size() == 3);
    debug.end_frame();
    CHECK(debug.size() == 1);
    debug.end_frame();
    CHECK(debug.size() == 1);
    debug.end_frame();
    CHECK(debug.size() == 0);
}

TEST_CASE("DebugRenderer: text needs a font")
{
    DebugFixture f;
    DebugRenderer debug;
    debug.text({ 0.0f, 0.0f }, "hello");
    CHECK(debug.size() == 0);
    debug.set_font(&f.font);
    CHECK(debug.font() == &f.font);
    debug.text({ 0.0f, 0.0f }, "hello");
    CHECK(debug.size() == 1);
}

TEST_CASE("DebugRenderer: render replays every shape in recording order")
{
    DebugFixture f;
    DebugRenderer debug;
    debug.set_font(&f.font);
    debug.line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    debug.rect({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    debug.circle({ 0.0f, 0.0f }, 2.0f, RED);
    debug.text({ 0.0f, 0.0f }, "AB");

    BatchRenderer2D batcher(f.desc());
    CHECK_THROWS_AS(debug.render(batcher), Error);
    batcher.begin(f.camera);
    debug.render(batcher);
    batcher.end();

    REQUIRE(f.sink.size() == 4);
    CHECK(f.sink[0].vertex_count == 2);
    CHECK(f.sink[1].vertex_count == 4);
    CHECK(f.sink[2].vertex_count == 4);
    CHECK(f.sink[3].vertex_count == 8);
    CHECK(batcher.stats().primitives == 1 + 1 + 1 + 2);
    CHECK(debug.size() == 4);
}

TEST_CASE("DebugRenderer: refuses shapes beyond the cap and counts them")
{
    DebugRenderer debug;
    for (size_t i = 0; i < DEBUG_MAX_COMMANDS; ++i)
    {
        debug.line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    }
    debug.line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    CHECK(debug.size() == DEBUG_MAX_COMMANDS);
    CHECK(debug.dropped() == 1);
}

TEST_CASE("Renderer: the scene replays debug shapes once in Scene2D and they age with the frame")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    Renderer::debug().line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED, 2);
    Camera2D camera(10.0f, 10.0f);

    test::render_2d(camera, [](BatchRenderer2D&) {});
    CHECK(Renderer::batch_stats().primitives == 1);
    Renderer::end_frame();
    CHECK(Renderer::debug().size() == 1);
    Renderer::end_frame();
    CHECK(Renderer::debug().size() == 0);
}

TEST_CASE("Renderer: the scene replays debug shapes even when no source draws")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    Renderer::debug().rect({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    Camera2D camera(10.0f, 10.0f);
    Renderer::scene().begin_scene(test::view_over(camera));
    Renderer::scene().end_scene();
    CHECK(Renderer::batch_stats().primitives == 1);
    Renderer::end_frame();
}

TEST_CASE("Renderer: draw_text batches one glyph quad per character through the scene")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    Camera2D camera(10.0f, 10.0f);

    CHECK_THROWS_AS(Renderer::scene().batcher_2d().draw_text({ 0.0f, 0.0f }, "A", font), Error);
    test::render_2d(camera, [&font](BatchRenderer2D& batcher) { batcher.draw_text({ 0.0f, 0.0f }, "AB", font); });
    CHECK(Renderer::batch_stats().primitives == 2);
    Renderer::end_frame();
    font.release_atlases();
}
