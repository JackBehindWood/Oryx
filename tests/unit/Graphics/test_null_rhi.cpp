#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"

using namespace oryx;

namespace
{

RHIViewportDesc viewport_desc()
{
    RHIViewportDesc desc;
    desc.width = 8;
    desc.height = 8;
    return desc;
}

} // namespace

TEST_CASE("NullRHI exposes the last submission and submit count")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
    RHIBufferPtr buffer = rhi.create_buffer({ .size = 32 });

    CHECK(rhi.submit_count() == 0);
    CHECK(rhi.last_submission().empty());

    RHICommandList list;
    list.begin_pass(back_buffer.get());
    list.set_vertex_buffer(0, buffer.get());
    list.end_pass();
    rhi.submit(list);
    rhi.present(*viewport);

    CHECK(rhi.submit_count() == 1);
    REQUIRE(rhi.last_submission().size() == 3);
    CHECK(rhi.last_submission()[0] == RHICommandType::BeginPass);
    CHECK(rhi.last_submission()[1] == RHICommandType::SetVertexBuffer);
    CHECK(rhi.last_submission()[2] == RHICommandType::EndPass);

    list.clear();
    list.begin_pass(back_buffer.get());
    list.end_pass();
    rhi.submit(list);
    CHECK(rhi.submit_count() == 2);
    CHECK(rhi.last_submission().size() == 2);
}

TEST_CASE("NullRHI submit rejects an unfinished pass and records nothing")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();

    RHICommandList list;
    list.begin_pass(back_buffer.get());
    CHECK_THROWS_AS(rhi.submit(list), Error);
    CHECK(rhi.submit_count() == 0);
}

TEST_CASE("NullRHI retains submitted resources until present")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
    RHIBufferPtr buffer = rhi.create_buffer({ .size = 16 });
    const NullBuffer* raw = static_cast<NullBuffer*>(buffer.get());

    RHICommandList list;
    list.begin_pass(back_buffer.get());
    list.set_vertex_buffer(0, buffer.get());
    list.end_pass();
    rhi.submit(list);
    CHECK(raw->ref_count() == 2);
    CHECK(list.empty());

    const size_t live_before = rhi.live_resources();
    buffer.reset();
    CHECK(rhi.live_resources() == live_before);

    rhi.present(*viewport);
    CHECK(rhi.live_resources() == live_before - 1);
}

TEST_CASE("NullRHI buffers store updates and clears write the colour")
{
    NullRHI rhi;
    RHIBufferPtr buffer = rhi.create_buffer({ .size = 4 });
    const std::array<uint8_t, 2> data = { uint8_t{ 7 }, uint8_t{ 9 } };
    buffer->update(1, data.data(), static_cast<uint32_t>(data.size()));
    const std::vector<uint8_t>& bytes = static_cast<NullBuffer&>(*buffer).bytes();
    CHECK(bytes[0] == uint8_t{ 0 });
    CHECK(bytes[1] == uint8_t{ 7 });
    CHECK(bytes[2] == uint8_t{ 9 });

    RHITexturePtr texture = rhi.create_texture({ .width = 1, .height = 1, .format = RHIFormat::BGRA8Unorm, .usage = RHITextureUsage::RenderTarget });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    RHICommandList list;
    list.begin_pass(target.get(), { Colour{ 1.0f, 0.0f, 0.0f, 1.0f }, true });
    list.end_pass();
    rhi.submit(list);

    std::array<uint8_t, 4> pixel{};
    rhi.read_texture(*texture, pixel.data(), static_cast<uint32_t>(pixel.size()));
    CHECK(pixel[0] == uint8_t{ 0 });
    CHECK(pixel[2] == uint8_t{ 255 });
    CHECK(pixel[3] == uint8_t{ 255 });
}

TEST_CASE("NullRHI rejects mismatched shader stages and unsupported render target formats")
{
    NullRHI rhi;
    CHECK_THROWS_AS(rhi.create_vertex_shader({ .stage = ShaderStage::Pixel }), Error);
    CHECK_THROWS_AS(rhi.create_pixel_shader({ .stage = ShaderStage::Compute }), Error);
    RHITexturePtr depth = rhi.create_texture({ .width = 2, .height = 2, .format = RHIFormat::Depth32Float, .usage = RHITextureUsage::RenderTarget });
    CHECK_THROWS_AS(rhi.create_render_target({ .colour = depth }), Error);
}

TEST_CASE("NullRHI collects released resources on the next present")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    {
        RHIBufferPtr buffer = rhi.create_buffer({ .size = 4 });
        RHITexturePtr texture = rhi.create_texture({ .width = 1, .height = 1, .usage = RHITextureUsage::RenderTarget });
        RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
        CHECK(rhi.live_resources() == 4);
    }
    CHECK(rhi.live_resources() == 4);
    rhi.present(*viewport);
    CHECK(rhi.live_resources() == 1);
}

TEST_CASE("NullRHI wait_idle completes in-flight frames without present")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    RHITexturePtr texture = rhi.create_texture({ .width = 2, .height = 2, .usage = RHITextureUsage::RenderTarget });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    const size_t baseline = rhi.live_resources();
    {
        RHIBufferPtr buffer = rhi.create_buffer({ .size = 16 });
        RHICommandList list;
        list.begin_pass(target.get());
        list.set_vertex_buffer(0, buffer.get());
        list.end_pass();
        rhi.submit(list);
    }
    CHECK(rhi.live_resources() == baseline + 1);
    rhi.wait_idle();
    CHECK(rhi.live_resources() == baseline);
}

TEST_CASE("NullRHI frame slots do not grow over many frames")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
    RHICommandList list;
    for (int32_t frame = 0; frame < 1000; ++frame)
    {
        RHIBufferPtr buffer = rhi.create_buffer({ .size = 8 });
        list.begin_pass(back_buffer.get());
        list.set_vertex_buffer(0, buffer.get());
        list.end_pass();
        rhi.submit(list);
        rhi.present(*viewport);
        CHECK(RHIResource::retired_pending() <= 1);
    }
    rhi.wait_idle();
    CHECK(rhi.live_resources() == 3);
}

TEST_CASE("NullRHI present without a back buffer still advances the frame")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    const uint64_t frame = RHIResource::frame_serial();
    CHECK_NOTHROW(rhi.present(*viewport));
    CHECK(RHIResource::frame_serial() == frame + 1);
    CHECK(RHIResource::retired_pending() == 0);
}

TEST_CASE("NullRHI present copies a source texture into the back buffer")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 2, .height = 1, .format = RHIFormat::RGBA8Unorm });
    const std::array<uint8_t, 8> pixels = { 1, 2, 3, 4, 5, 6, 7, 8 };
    RHITexturePtr source = rhi.create_texture({ .width = 2, .height = 1, .format = RHIFormat::RGBA8Unorm, .initial_data = pixels.data(), .initial_data_size = static_cast<uint32_t>(pixels.size()) });

    rhi.present(*viewport, source.get());

    RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
    const NullRenderTarget& target = static_cast<NullRenderTarget&>(*back_buffer);
    CHECK(target.texture().bytes() == std::vector<uint8_t>(pixels.begin(), pixels.end()));
}

TEST_CASE("NullRHI present rejects a mismatched or unsampled source")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 2, .height = 2, .format = RHIFormat::RGBA8Unorm });
    RHITexturePtr wrong_size = rhi.create_texture({ .width = 3, .height = 2, .format = RHIFormat::RGBA8Unorm });
    RHITexturePtr wrong_format = rhi.create_texture({ .width = 2, .height = 2, .format = RHIFormat::BGRA8Unorm });
    RHITexturePtr unsampled = rhi.create_texture({ .width = 2, .height = 2, .format = RHIFormat::RGBA8Unorm, .usage = RHITextureUsage::RenderTarget });
    CHECK_THROWS_AS(rhi.present(*viewport, wrong_size.get()), Error);
    CHECK_THROWS_AS(rhi.present(*viewport, wrong_format.get()), Error);
    CHECK_THROWS_AS(rhi.present(*viewport, unsampled.get()), Error);
}

TEST_CASE("Two NullRHI devices can be alive at once")
{
    NullRHI first;
    NullRHI second;
    RHIBufferPtr buffer = first.create_buffer({ .size = 4 });
    RHIViewportPtr viewport = second.create_viewport(viewport_desc());
    buffer.reset();
    second.present(*viewport);
    CHECK(RHIResource::retired_pending() == 0);
}
