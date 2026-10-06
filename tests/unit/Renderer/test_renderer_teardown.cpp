#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "unit/Renderer/RenderTestSupport.h"
#include "unit/TestLogCapture.h"

using namespace oryx;

namespace
{

void run_frames(NullRHI& rhi, uint32_t frames)
{
    Texture2D sprite = Texture2D::create(rhi, { .width = 2, .height = 2 });
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        test::render_2d(Camera2D::screen_space(64.0f, 64.0f), [&sprite](BatchRenderer2D& batcher)
        {
            batcher.draw_rect({ 4.0f, 4.0f }, { 8.0f, 8.0f }, { 1.0f, 0.0f, 0.0f, 1.0f });
            batcher.draw_circle({ 32.0f, 32.0f }, 6.0f, { 0.0f, 1.0f, 0.0f, 1.0f });
            batcher.draw_sprite({ 20.0f, 20.0f }, { 8.0f, 8.0f }, sprite);
            batcher.draw_text({ 2.0f, 50.0f }, "Oryx", Renderer::default_font());
        });
        Renderer::end_frame();
    }
    (void)rhi;
}

} // namespace

TEST_CASE("Renderer teardown leaves no RHI resource alive across repeated init, frame and shutdown cycles")
{
    for (int32_t cycle = 0; cycle < 3; ++cycle)
    {
        Renderer::init({ RHIBackend::Null });
        NullRHI& rhi = static_cast<NullRHI&>(Renderer::rhi());
        RHIViewportPtr viewport = rhi.create_viewport({ .width = 64, .height = 64 });
        Renderer::set_viewport(viewport);
        run_frames(rhi, 5);
        CHECK(RHIResource::live_count() > 0);

        viewport.reset();
        Renderer::set_viewport({});
        Renderer::shutdown();
        CHECK(RHIResource::live_count() == 0);
        CHECK(RHIResource::retired_pending() == 0);
    }
}

TEST_CASE("Renderer teardown stays clean after a failed end_frame")
{
    Renderer::init({ RHIBackend::Null });
    NullRHI& rhi = static_cast<NullRHI&>(Renderer::rhi());
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 64, .height = 64 });
    Renderer::set_viewport(viewport);
    run_frames(rhi, 2);

    rhi.fail_next_end_frame();
    CHECK_THROWS_AS(Renderer::end_frame(), Error);

    viewport.reset();
    Renderer::set_viewport({});
    Renderer::shutdown();
    CHECK(RHIResource::live_count() == 0);
}

TEST_CASE("RHIResource::live_report groups live resources by type")
{
    CHECK(RHIResource::live_report().empty());
    NullRHI rhi;
    RHIBufferPtr first = rhi.create_buffer({ .size = 16 });
    RHIBufferPtr second = rhi.create_buffer({ .size = 16 });
    const std::string report = RHIResource::live_report();
    CHECK(report.find("2 x ") != std::string::npos);
    CHECK(report.find("NullBuffer") != std::string::npos);
}
