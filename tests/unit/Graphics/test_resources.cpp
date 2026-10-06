#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

namespace
{

struct Vertex2
{
    float x;
    float y;
};

struct Vertex3
{
    float x;
    float y;
    float z;
};

RHIVertexDeclaration layout_for_vertex2()
{
    return RHIVertexDeclarationBuilder().stream(0, sizeof(Vertex2)).attribute(0, RHIVertexFormat::Float2, 0).build();
}

void run_resource_checks(IRHI& rhi)
{
    const uint32_t ring = rhi.capabilities().frames_in_flight;

    SUBCASE("vertex buffer validates the element type, capacity and slot")
    {
        VertexBuffer buffer = VertexBuffer::create(rhi, layout_for_vertex2(), 4, BufferMode::Dynamic);
        const Vertex2 good[4] = {};
        const Vertex3 wrong_stride[2] = {};
        CHECK_NOTHROW(buffer.set_data(0, good));
        CHECK(buffer.vertex_count() == 4);
        CHECK_THROWS_AS(buffer.set_data(0, wrong_stride), Error);
        CHECK_THROWS_AS(buffer.set_data(0, good, 5), Error);
        CHECK_THROWS_AS(buffer.set_data(ring, good), Error);
        CHECK(buffer.region_count() == ring);
    }

    SUBCASE("static vertex buffer is written once")
    {
        VertexBuffer buffer = VertexBuffer::create(rhi, layout_for_vertex2(), 3, BufferMode::Static);
        const Vertex2 data[3] = {};
        CHECK(buffer.region_count() == 1);
        CHECK_NOTHROW(buffer.set_data(rhi.frame_slot(), data));
        CHECK_THROWS_AS(buffer.set_data(rhi.frame_slot(), data), Error);
        CHECK(buffer.offset(rhi.frame_slot()) == 0);
    }

    SUBCASE("dynamic regions rotate across end_frame without overlapping")
    {
        VertexBuffer buffer = VertexBuffer::create(rhi, layout_for_vertex2(), 8, BufferMode::Dynamic);
        std::vector<uint32_t> offsets;
        for (uint32_t frame = 0; frame < ring * 2; ++frame)
        {
            offsets.push_back(buffer.offset(rhi.frame_slot()));
            rhi.end_frame();
        }
        for (uint32_t i = 0; i < ring * 2; ++i)
        {
            CHECK(offsets[i] == offsets[i % ring]);
            for (uint32_t j = 0; j < ring; ++j)
            {
                CHECK((i % ring == j) == (offsets[i] == offsets[j]));
            }
        }
        CHECK(buffer.rhi().size() == 8 * sizeof(Vertex2) * ring);
    }

    SUBCASE("index buffer supports both widths and rejects the wrong one")
    {
        IndexBuffer small = IndexBuffer::create(rhi, IndexType::U16, 6, BufferMode::Dynamic);
        const uint16_t indices16[6] = { 0, 1, 2, 2, 1, 3 };
        const uint32_t indices32[3] = { 0, 1, 2 };
        CHECK_FALSE(small.index32());
        CHECK_NOTHROW(small.set_data(0, indices16));
        CHECK(small.index_count() == 6);
        CHECK_THROWS_AS(small.set_data(0, indices32), Error);
        CHECK_THROWS_AS(small.set_data(0, indices16, 7), Error);

        IndexBuffer wide = IndexBuffer::create(rhi, IndexType::U32, 3, BufferMode::Static);
        CHECK(wide.index32());
        CHECK_NOTHROW(wide.set_data(0, indices32));
        CHECK_THROWS_AS(wide.set_data(0, indices32), Error);
        CHECK_THROWS_AS(wide.set_data(0, indices16, 3), Error);
    }

    SUBCASE("map, upload_buffer and copy_buffer round trip through a GpuOnly buffer")
    {
        const uint8_t data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        RHIBufferPtr device = rhi.create_buffer({ .size = 16, .usage = RHIBufferUsage::Vertex | RHIBufferUsage::CopySource | RHIBufferUsage::CopyDest, .memory = RHIMemory::GpuOnly });
        RHIBufferPtr readback = rhi.create_buffer({ .size = 16, .usage = RHIBufferUsage::CopyDest });
        RHIBufferPtr staging = rhi.create_buffer({ .size = 16, .usage = RHIBufferUsage::CopySource });

        CHECK_THROWS_AS(device->map(), Error);
        uint8_t* mapped = staging->map();
        REQUIRE(mapped != nullptr);
        CHECK(staging->map() == mapped);
        std::memcpy(mapped + 4, data, sizeof(data));

        rhi.upload_buffer(device.get(), 0, data, 4);
        CHECK_THROWS_AS(rhi.upload_buffer(device.get(), 14, data, 4), Error);

        RHICommandList list;
        list.copy_buffer(staging.get(), 4, device.get(), 4, 4);
        list.copy_buffer(device.get(), 0, readback.get(), 8, 8);
        rhi.submit(list);
        rhi.wait_idle();

        const uint8_t expected[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        CHECK(std::memcmp(readback->map() + 8, expected, 4) == 0);
        CHECK(std::memcmp(readback->map() + 12, expected, 4) == 0);
    }

    SUBCASE("uniform buffer rejects oversized data")
    {
        UniformBuffer buffer = UniformBuffer::create(rhi, 16);
        const float four[4] = {};
        const float five[5] = {};
        CHECK_NOTHROW(buffer.set_data(0, four));
        CHECK_THROWS_AS(buffer.set_data(0, five), Error);
        CHECK_THROWS_AS(UniformBuffer::create(rhi, 0), Error);
    }

    SUBCASE("texture 2d round trips its pixels")
    {
        const uint8_t pixels[2 * 2 * 4] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
        Texture2D texture = Texture2D::create(rhi, { .width = 2, .height = 2, .pixels = pixels, .pixel_bytes = sizeof(pixels), .filter = RHIFilter::Nearest });
        CHECK(texture.width() == 2);
        CHECK(texture.sampler()->min_filter() == RHIFilter::Nearest);
        uint8_t readback[sizeof(pixels)] = {};
        rhi.read_texture(&texture.rhi(), readback, sizeof(readback));
        CHECK(std::memcmp(pixels, readback, sizeof(pixels)) == 0);
        CHECK_THROWS_AS(Texture2D::create(rhi, { .width = 2, .height = 2, .pixels = pixels, .pixel_bytes = 3 }), Error);
    }

    SUBCASE("render target clears into a readable colour texture")
    {
        RenderTarget target = RenderTarget::create(rhi, 3, 2);
        CHECK(target.width() == 3);
        CHECK(target.colour().height() == 2);
        RHICommandList list;
        list.begin_pass(&target.rhi(), { Colour{ 1.0f, 0.0f, 0.0f, 1.0f }, true });
        list.end_pass();
        rhi.submit(list);
        uint8_t readback[3 * 2 * 4] = {};
        rhi.read_texture(&target.colour().rhi(), readback, sizeof(readback));
        for (uint32_t pixel = 0; pixel < 6; ++pixel)
        {
            CHECK(readback[pixel * 4 + 0] == 255);
            CHECK(readback[pixel * 4 + 1] == 0);
            CHECK(readback[pixel * 4 + 3] == 255);
        }
    }
}

} // namespace

TEST_CASE("Resources: Null backend")
{
    UniquePtr<IRHI> rhi = create_rhi(RHIBackend::Null);
    run_resource_checks(*rhi);
}

#if defined(OX_PLATFORM_MACOS) && defined(OX_ENABLE_GRAPHICS)
TEST_CASE("Resources: Metal backend")
{
    UniquePtr<IRHI> rhi;
    try
    {
        rhi = create_rhi(RHIBackend::Metal);
    }
    catch (const Error&)
    {
        return;
    }
    run_resource_checks(*rhi);
}
#endif

TEST_CASE("Resources: wrappers on the Renderer device")
{
    Renderer::init({ RHIBackend::Null });
    {
        VertexBuffer vertices = VertexBuffer::create(Renderer::rhi(), layout_for_vertex2(), 4, BufferMode::Dynamic);
        IndexBuffer indices = IndexBuffer::create(Renderer::rhi(), IndexType::U16, 6, BufferMode::Static);
        UniformBuffer uniform = UniformBuffer::create(Renderer::rhi(), 64);
        Texture2D texture = Texture2D::create(Renderer::rhi(), { .width = 1, .height = 1 });
        RenderTarget target = RenderTarget::create(Renderer::rhi(), 4, 4);
        CHECK(vertices.region_count() == Renderer::rhi().capabilities().frames_in_flight);
        CHECK(Renderer::frame_slot() < Renderer::rhi().capabilities().frames_in_flight);
        CHECK(indices.capacity() == 6);
        CHECK(uniform.size() == 64);
        CHECK(texture.width() == 1);
        CHECK(target.width() == 4);
    }
    Renderer::shutdown();
}
