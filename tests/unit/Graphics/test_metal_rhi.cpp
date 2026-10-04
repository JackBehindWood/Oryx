#include "doctest.h"

#include "rhi_contract.h"

#if defined(OX_PLATFORM_MACOS) && defined(OX_ENABLE_GRAPHICS)

using namespace oryx;

namespace
{

UniquePtr<IRHI> try_create_metal()
{
    try
    {
        return create_rhi(RHIBackend::Metal);
    }
    catch (const Error& error)
    {
        MESSAGE("Metal unavailable, skipping: ", error.what());
        return {};
    }
}

void clear_offscreen(IRHI& rhi, RHIFormat format, const Colour& colour, std::vector<uint8_t>& pixels)
{
    RHITexturePtr texture = rhi.create_texture({ .width = 5, .height = 3, .format = format, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    RHICommandList list;
    list.begin_pass(target.get(), { colour, true });
    list.end_pass();
    rhi.submit(list);
    pixels.assign(5 * 3 * 4, 0);
    rhi.read_texture(*texture, pixels.data(), static_cast<uint32_t>(pixels.size()));
}

} // namespace

TEST_CASE("Metal RHI: contract")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    CHECK(rhi->backend() == RHIBackend::Metal);
    CHECK(rhi->capabilities().frames_in_flight == 3);
    oryx::test::run_rhi_contract(*rhi);
}

TEST_CASE("Metal RHI: offscreen clear is read back exactly")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    const Colour colour{ 1.0f, 0.0f, 51.0f / 255.0f, 128.0f / 255.0f };

    std::vector<uint8_t> pixels;
    clear_offscreen(*rhi, RHIFormat::RGBA8Unorm, colour, pixels);
    for (size_t pixel = 0; pixel < 15; ++pixel)
    {
        CHECK(pixels[pixel * 4 + 0] == 255);
        CHECK(pixels[pixel * 4 + 1] == 0);
        CHECK(pixels[pixel * 4 + 2] == 51);
        CHECK(pixels[pixel * 4 + 3] == 128);
    }

    clear_offscreen(*rhi, RHIFormat::BGRA8Unorm, colour, pixels);
    for (size_t pixel = 0; pixel < 15; ++pixel)
    {
        CHECK(pixels[pixel * 4 + 0] == 51);
        CHECK(pixels[pixel * 4 + 1] == 0);
        CHECK(pixels[pixel * 4 + 2] == 255);
        CHECK(pixels[pixel * 4 + 3] == 128);
    }
}

TEST_CASE("Metal RHI: frames in flight with mid-flight drops leak nothing")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    const size_t baseline = RHIResource::live_count();
    {
        RHIViewportPtr viewport = rhi->create_viewport({ .width = 16, .height = 16 });
        for (uint32_t frame = 0; frame < 24; ++frame)
        {
            RHIBufferPtr buffer = rhi->create_buffer({ .size = 64 });
            RHITexturePtr texture = rhi->create_texture({ .width = 4, .height = 4 });
            RHISamplerPtr sampler = rhi->create_sampler({});
            RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
            REQUIRE(back_buffer);
            RHICommandList list;
            list.begin_pass(back_buffer.get(), { Colour{ 0.2f, 0.4f, 0.6f, 1.0f }, true });
            list.set_vertex_buffer(0, buffer.get());
            list.end_pass();
            rhi->submit(list);
            rhi->present(*viewport);
            rhi->end_frame();
        }
        rhi->wait_idle();
    }
    rhi->wait_idle();
    CHECK(RHIResource::live_count() == baseline);
    CHECK(RHIResource::retired_pending() == 0);
}

TEST_CASE("Metal RHI: pipelines and draws are not implemented yet")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    CHECK_THROWS_AS(rhi->create_vertex_shader({}), Error);
    CHECK_THROWS_AS(rhi->create_pixel_shader({ .stage = ShaderStage::Pixel }), Error);
}

TEST_CASE("Metal RHI: present copies a source texture into the viewport")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    RHIViewportPtr viewport = rhi->create_viewport({ .width = 4, .height = 4 });
    RHITexturePtr source = rhi->create_texture({ .width = 4, .height = 4, .format = RHIFormat::BGRA8Unorm, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
    RHITexturePtr wrong_size = rhi->create_texture({ .width = 2, .height = 2, .format = RHIFormat::BGRA8Unorm });
    CHECK_THROWS_AS(rhi->present(*viewport, wrong_size.get()), Error);

    RHIRenderTargetPtr target = rhi->create_render_target({ .colour = source });
    RHICommandList list;
    list.begin_pass(target.get(), { Colour{ 0.0f, 1.0f, 0.0f, 1.0f }, true });
    list.end_pass();
    rhi->submit(list);
    CHECK_NOTHROW(rhi->present(*viewport, source.get()));
    rhi->end_frame();
    rhi->wait_idle();
}

#else

TEST_CASE("Metal RHI: not built on this platform")
{
}

#endif
