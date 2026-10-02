#include "doctest.h"

#include "rhi_contract.h"
#include "NullRHI.h"

using namespace oryx;

namespace
{

std::vector<uint8_t> make_bytes(size_t count, uint8_t seed)
{
    std::vector<uint8_t> bytes(count);
    for (size_t i = 0; i < count; ++i)
    {
        bytes[i] = static_cast<uint8_t>(seed + i * 7);
    }
    return bytes;
}

RHIViewportDesc viewport_desc(int32_t width, int32_t height)
{
    RHIViewportDesc desc;
    desc.width = static_cast<uint32_t>(width);
    desc.height = static_cast<uint32_t>(height);
    return desc;
}

void clear_and_submit(IRHI& rhi, RHIRenderTarget& target, const Colour& colour)
{
    RHICommandList list;
    list.begin_pass(&target, { colour, true });
    list.end_pass();
    rhi.submit(list);
}

} // namespace

namespace oryx::test
{

void run_rhi_contract(IRHI& rhi)
{
    const RHICapabilities& caps = rhi.capabilities();
    REQUIRE(caps.frames_in_flight >= 1);
    REQUIRE(caps.max_texture_size >= 64);

    SUBCASE("buffer create and update")
    {
        const std::vector<uint8_t> initial = make_bytes(16, 1);
        RHIBufferPtr buffer = rhi.create_buffer({ .size = 16, .usage = RHIBufferUsage::Vertex | RHIBufferUsage::Uniform, .initial_data = initial.data(), .initial_data_size = static_cast<uint32_t>(initial.size()) });
        REQUIRE(buffer);
        CHECK(buffer->size() == 16);
        CHECK(buffer->usage() == (RHIBufferUsage::Vertex | RHIBufferUsage::Uniform));
        CHECK(buffer->memory() == RHIMemory::CpuToGpu);
        CHECK_NOTHROW(buffer->update(4, initial.data(), 8));
        CHECK_THROWS_AS(buffer->update(12, initial.data(), 8), Error);
        CHECK_THROWS_AS(buffer->update(17, initial.data(), 1), Error);

        RHIBufferPtr gpu_only = rhi.create_buffer({ .size = 16, .usage = RHIBufferUsage::Index, .memory = RHIMemory::GpuOnly });
        CHECK_THROWS_AS(gpu_only->update(0, initial.data(), 4), Error);
        CHECK_THROWS_AS(rhi.create_buffer({ .size = 0 }), Error);
    }

    SUBCASE("texture create and read_texture round trip")
    {
        const std::vector<uint8_t> pixels = make_bytes(4 * 3 * 4, 9);
        RHITexturePtr texture = rhi.create_texture({ .width = 4, .height = 3, .format = RHIFormat::RGBA8Unorm, .initial_data = pixels.data(), .initial_data_size = static_cast<uint32_t>(pixels.size()) });
        REQUIRE(texture);
        CHECK(texture->width() == 4);
        CHECK(texture->height() == 3);
        CHECK(texture->format() == RHIFormat::RGBA8Unorm);

        std::vector<uint8_t> readback(pixels.size());
        rhi.read_texture(*texture, readback.data(), static_cast<uint32_t>(readback.size()));
        CHECK(readback == pixels);

        std::vector<uint8_t> wrong_size(pixels.size() - 1);
        CHECK_THROWS_AS(rhi.read_texture(*texture, wrong_size.data(), static_cast<uint32_t>(wrong_size.size())), Error);
        CHECK_THROWS_AS(rhi.create_texture({ .width = 0, .height = 3 }), Error);
        CHECK_THROWS_AS(rhi.create_texture({ .width = 4, .height = 3, .initial_data = pixels.data(), .initial_data_size = 4 }), Error);
    }

    SUBCASE("sampler")
    {
        RHISamplerPtr sampler = rhi.create_sampler({ .min_filter = RHIFilter::Nearest, .address_u = RHIAddressMode::Repeat });
        REQUIRE(sampler);
        CHECK(sampler->min_filter() == RHIFilter::Nearest);
        CHECK(sampler->address_u() == RHIAddressMode::Repeat);
    }

    SUBCASE("offscreen render target clear is read back exactly")
    {
        RHITexturePtr texture = rhi.create_texture({ .width = 4, .height = 2, .format = RHIFormat::RGBA8Unorm, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
        RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
        REQUIRE(target);
        CHECK(target->width() == 4);
        CHECK(target->height() == 2);
        CHECK(target->format() == RHIFormat::RGBA8Unorm);
        CHECK(target->colour().get() == texture.get());

        clear_and_submit(rhi, *target, Colour{ 1.0f, 0.0f, 51.0f / 255.0f, 128.0f / 255.0f });
        std::vector<uint8_t> readback(4 * 2 * 4);
        rhi.read_texture(*texture, readback.data(), static_cast<uint32_t>(readback.size()));
        for (size_t pixel = 0; pixel < 8; ++pixel)
        {
            CHECK(readback[pixel * 4 + 0] == uint8_t{ 255 });
            CHECK(readback[pixel * 4 + 1] == uint8_t{ 0 });
            CHECK(readback[pixel * 4 + 2] == uint8_t{ 51 });
            CHECK(readback[pixel * 4 + 3] == uint8_t{ 128 });
        }

        RHITexturePtr sampled_only = rhi.create_texture({ .width = 4, .height = 2 });
        CHECK_THROWS_AS(rhi.create_render_target({ .colour = sampled_only }), Error);
        CHECK_THROWS_AS(rhi.create_render_target({}), Error);
    }

    SUBCASE("viewport create, zero size and resize")
    {
        RHIViewportPtr viewport = rhi.create_viewport(viewport_desc(64, 32));
        REQUIRE(viewport);
        CHECK(viewport->width() == 64);
        CHECK(viewport->height() == 32);

        RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
        REQUIRE(back_buffer);
        CHECK(back_buffer->width() == 64);
        CHECK(back_buffer->height() == 32);
        CHECK(viewport->acquire_back_buffer().get() == back_buffer.get());

        viewport->resize(0, 0, 1.0f);
        CHECK(viewport->width() == 0);
        CHECK_FALSE(viewport->acquire_back_buffer());

        viewport->resize(16, 8, 2.0f);
        RHIRenderTargetPtr resized = viewport->acquire_back_buffer();
        REQUIRE(resized);
        CHECK(resized->width() == 16);
        CHECK(resized->height() == 8);

        RHIViewportPtr empty = rhi.create_viewport(viewport_desc(0, 0));
        CHECK_FALSE(empty->acquire_back_buffer());
    }

    SUBCASE("many frames of submit and present leak nothing")
    {
        RHIViewportPtr viewport = rhi.create_viewport(viewport_desc(8, 8));
        rhi.wait_idle();
        const uint32_t frames = caps.frames_in_flight + 3;
        for (uint32_t frame = 0; frame < frames; ++frame)
        {
            RHIBufferPtr buffer = rhi.create_buffer({ .size = 16 });
            RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
            REQUIRE(back_buffer);
            RHICommandList list;
            list.begin_pass(back_buffer.get(), { Colour{ 0.0f, 0.0f, 1.0f, 1.0f }, true });
            list.set_vertex_buffer(0, buffer.get());
            list.end_pass();
            rhi.submit(list);
            rhi.present(*viewport);
        }
        rhi.wait_idle();
        viewport.reset();
        rhi.present(*rhi.create_viewport(viewport_desc(1, 1)));
        rhi.wait_idle();
    }

    SUBCASE("resource dropped mid-flight survives until its frame completes")
    {
        RHIViewportPtr viewport = rhi.create_viewport(viewport_desc(4, 4));
        RHITexturePtr texture = rhi.create_texture({ .width = 2, .height = 2, .usage = RHITextureUsage::RenderTarget });
        RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
        RHITexture* raw_texture = texture.get();

        RHICommandList list;
        list.begin_pass(target.get(), { Colour{ 0.0f, 1.0f, 0.0f, 1.0f }, true });
        list.end_pass();
        rhi.submit(list);

        target.reset();
        texture.reset();
        CHECK(raw_texture->ref_count() >= 1);
        CHECK(raw_texture->width() == 2);

        rhi.present(*viewport);
        rhi.wait_idle();
    }
}

} // namespace oryx::test

TEST_CASE("RHI contract: Null backend")
{
    UniquePtr<IRHI> rhi = create_rhi(RHIBackend::Null);
    REQUIRE(rhi);
    CHECK(rhi->backend() == RHIBackend::Null);
    oryx::test::run_rhi_contract(*rhi);
}

TEST_CASE("RHI contract: unimplemented backends throw Error")
{
    for (RHIBackend backend : { RHIBackend::Metal, RHIBackend::OpenGL, RHIBackend::Vulkan, RHIBackend::D3D12, RHIBackend::WebGPU })
    {
        CAPTURE(to_string(backend));
        CHECK_THROWS_AS(create_rhi(backend), Error);
    }
    CHECK_THROWS_WITH_AS(create_rhi(RHIBackend::Vulkan), "Vulkan is not implemented", Error);
}

TEST_CASE("RHIBackend names round-trip")
{
    for (RHIBackend backend : { RHIBackend::Null, RHIBackend::Metal, RHIBackend::OpenGL, RHIBackend::Vulkan, RHIBackend::D3D12, RHIBackend::WebGPU })
    {
        CHECK(parse_rhi_backend(to_string(backend)) == backend);
    }
    CHECK(parse_rhi_backend("metal") == RHIBackend::Metal);
    CHECK_THROWS_AS(parse_rhi_backend("directx"), Error);
}

TEST_CASE("default_rhi_backend matches the platform")
{
#ifdef OX_PLATFORM_MACOS
    CHECK(default_rhi_backend() == RHIBackend::Metal);
#else
    CHECK(default_rhi_backend() == RHIBackend::Null);
#endif
}
