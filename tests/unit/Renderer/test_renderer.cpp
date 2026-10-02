#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"

using namespace oryx;

namespace
{

struct RendererGuard
{
    ~RendererGuard() { Renderer::shutdown(); }
};

} // namespace

TEST_CASE("Renderer: init, queued frame and shutdown")
{
    RendererGuard guard;
    CHECK_FALSE(Renderer::initialised());
    CHECK_THROWS_AS(Renderer::rhi(), Error);
    CHECK_THROWS_AS(Renderer::clear({}), Error);

    Renderer::init({ RHIBackend::Null });
    CHECK(Renderer::initialised());
    CHECK_THROWS_AS(Renderer::init({ RHIBackend::Null }), Error);
    CHECK(Renderer::rhi().backend() == RHIBackend::Null);

    NullRHI& rhi = static_cast<NullRHI&>(Renderer::rhi());
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 8, .height = 8 });
    Renderer::set_viewport(viewport);

    Renderer::end_frame();
    CHECK(rhi.submit_count() == 0);

    Renderer::clear(Colour{ 1.0f, 0.0f, 0.0f, 1.0f });
    Renderer::end_frame();
    CHECK(rhi.submit_count() == 1);
    Renderer::end_frame();
    CHECK(rhi.submit_count() == 1);

    viewport.reset();
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
    Renderer::clear({});
    Renderer::end_frame();
    CHECK(rhi.submit_count() == 0);
    viewport.reset();
}

TEST_CASE("Renderer: several clears in one frame record one pass each, in order")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    NullRHI& rhi = static_cast<NullRHI&>(Renderer::rhi());
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 4, .height = 4 });
    Renderer::set_viewport(viewport);

    Renderer::clear(Colour{ 1.0f, 0.0f, 0.0f, 1.0f });
    Renderer::clear(Colour{ 0.0f, 1.0f, 0.0f, 1.0f });
    Renderer::end_frame();
    CHECK(rhi.submit_count() == 1);
    CHECK(rhi.last_submission().size() == 4);

    Renderer::end_frame();
    CHECK(rhi.submit_count() == 1);
    viewport.reset();
}

TEST_CASE("Renderer: begin_frame discards queued work and no viewport presents nothing")
{
    RendererGuard guard;
    Renderer::init({ RHIBackend::Null });
    NullRHI& rhi = static_cast<NullRHI&>(Renderer::rhi());

    Renderer::clear({});
    Renderer::end_frame();
    CHECK(rhi.submit_count() == 0);

    RHIViewportPtr viewport = rhi.create_viewport({ .width = 4, .height = 4 });
    Renderer::set_viewport(viewport);
    Renderer::clear({});
    Renderer::begin_frame();
    Renderer::end_frame();
    CHECK(rhi.submit_count() == 0);
    viewport.reset();
}
