#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"

using namespace oryx;

namespace
{

struct RendererGuard
{
    RendererGuard() { Renderer::init({ RHIBackend::Null }); }
    ~RendererGuard() { Renderer::shutdown(); }
};

} // namespace

TEST_CASE("ScreenScene opens a scene for its lifetime and closes it on exit")
{
    RendererGuard guard;
    {
        ScreenScene scene(320.0f, 200.0f);
        CHECK(Renderer::batcher_2d().open());
        Renderer::draw_rect({ 10.0f, 10.0f }, { 5.0f, 5.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
    }
    CHECK_FALSE(Renderer::batcher_2d().open());
    CHECK(Renderer::batch_stats().primitives == 1);
}

TEST_CASE("ScreenScene takes any camera and refuses to nest")
{
    RendererGuard guard;
    ScreenScene outer(Camera2D(10.0f, 10.0f));
    CHECK_THROWS_AS(ScreenScene(10.0f, 10.0f), Error);
    CHECK(Renderer::batcher_2d().open());
}

TEST_CASE("ScreenScene throws when the scene was closed behind its back, but not while unwinding")
{
    RendererGuard guard;
    CHECK_THROWS_AS(([] {
        ScreenScene scene(10.0f, 10.0f);
        Renderer::end_scene();
    }()), Error);
    CHECK_FALSE(Renderer::batcher_2d().open());

    auto unwind = []
    {
        ScreenScene scene(10.0f, 10.0f);
        Renderer::end_scene();
        throw Error("draw failed");
    };
    CHECK_THROWS_WITH_AS(unwind(), "draw failed", Error);
}
