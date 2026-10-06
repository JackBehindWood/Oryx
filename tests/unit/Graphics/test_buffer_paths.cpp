#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "Oryx/Renderer/DefaultResources.h"

using namespace oryx;

namespace
{

struct Vertex2
{
    float x;
    float y;
};

RHIVertexDeclaration layout_for_vertex2()
{
    return RHIVertexDeclarationBuilder().stream(0, sizeof(Vertex2)).attribute(0, RHIVertexFormat::Float2, 0).build();
}

const std::vector<uint8_t>& bytes_of(const RHIBufferPtr& buffer)
{
    return static_cast<NullBuffer&>(*buffer).bytes();
}

} // namespace

TEST_CASE("checked_buffer_bytes multiplies within the 32-bit limit and throws past it")
{
    CHECK(checked_buffer_bytes(4, 8, 3) == 96);
    CHECK(checked_buffer_bytes(0, 8, 3) == 0);
    CHECK(checked_buffer_bytes(0xFFFFFFFFu, 1, 1) == 0xFFFFFFFFu);
    CHECK_THROWS_AS(checked_buffer_bytes(0x80000000u, 2, 1), Error);
    CHECK_THROWS_AS(checked_buffer_bytes(0x10000u, 0x10000u, 1), Error);
    CHECK_THROWS_AS(checked_buffer_bytes(0x40000000u, 1, 4), Error);
    CHECK_THROWS_AS(checked_buffer_bytes(std::numeric_limits<size_t>::max(), 2, 1), Error);
}

TEST_CASE("Buffer creation throws instead of wrapping a 32-bit size")
{
    NullRHI rhi;
    const uint32_t regions = rhi.capabilities().frames_in_flight;
    const uint32_t huge = 0xFFFFFFFFu / regions + 1;
    CHECK_THROWS_AS(TransientAllocator::create(rhi, 0xFFFFFFFFu, RHIBufferUsage::Vertex), Error);
    CHECK_THROWS_AS(TransientAllocator::create(rhi, huge, RHIBufferUsage::Vertex), Error);
    CHECK_THROWS_AS(VertexBuffer::create(rhi, layout_for_vertex2(), 0x20000000u, BufferMode::Static), Error);
    CHECK_THROWS_AS(VertexBuffer::create(rhi, layout_for_vertex2(), huge, BufferMode::Dynamic), Error);
    CHECK_THROWS_AS(IndexBuffer::create(rhi, IndexType::U32, 0x40000000u, BufferMode::Static), Error);
    CHECK_THROWS_AS(IndexBuffer::create(rhi, IndexType::U16, huge, BufferMode::Dynamic), Error);
    CHECK_THROWS_AS(UniformBuffer::create(rhi, 0xFFFFFFFFu), Error);
}

TEST_CASE("UniformBuffer has one aligned region per frame in flight")
{
    NullRHI rhi;
    UniformBuffer buffer = UniformBuffer::create(rhi, 24);
    REQUIRE(buffer.region_count() == rhi.capabilities().frames_in_flight);
    CHECK(buffer.region_size() == UniformBuffer::REGION_ALIGNMENT);
    CHECK(buffer.rhi().size() == buffer.region_size() * buffer.region_count());
    for (uint32_t slot = 0; slot < buffer.region_count(); ++slot)
    {
        CHECK(buffer.offset(slot) == slot * buffer.region_size());
    }
    CHECK_THROWS_AS(buffer.offset(buffer.region_count()), Error);

    for (uint32_t slot = 0; slot < buffer.region_count(); ++slot)
    {
        const float value[6] = { static_cast<float>(slot + 1) };
        buffer.set_data(slot, value);
    }
    for (uint32_t slot = 0; slot < buffer.region_count(); ++slot)
    {
        float read = 0.0f;
        std::memcpy(&read, bytes_of(buffer.rhi_ptr()).data() + buffer.offset(slot), sizeof(read));
        CHECK(read == static_cast<float>(slot + 1));
    }
    const float seven[7] = {};
    CHECK_THROWS_AS(buffer.set_data(0, seven), Error);
    CHECK_THROWS_AS(buffer.set_data(buffer.region_count(), 1.0f), Error);
}

TEST_CASE("RHIBuffer::map is persistent for CpuToGpu buffers and throws for GpuOnly")
{
    NullRHI rhi;
    RHIBufferPtr upload = rhi.create_buffer({ .size = 16 });
    uint8_t* mapped = upload->map();
    REQUIRE(mapped != nullptr);
    CHECK(upload->map() == mapped);
    mapped[3] = 42;
    CHECK(bytes_of(upload)[3] == 42);

    RHIBufferPtr device = rhi.create_buffer({ .size = 16, .memory = RHIMemory::GpuOnly });
    CHECK_THROWS_AS(device->map(), Error);
}

TEST_CASE("IRHI::upload_buffer writes both memory kinds and validates the range")
{
    NullRHI rhi;
    const uint8_t data[4] = { 1, 2, 3, 4 };
    for (RHIMemory memory : { RHIMemory::CpuToGpu, RHIMemory::GpuOnly })
    {
        RHIBufferPtr buffer = rhi.create_buffer({ .size = 8, .memory = memory });
        rhi.upload_buffer(buffer.get(), 2, data, sizeof(data));
        CHECK(bytes_of(buffer)[2] == 1);
        CHECK(bytes_of(buffer)[5] == 4);
        CHECK_THROWS_AS(rhi.upload_buffer(buffer.get(), 5, data, sizeof(data)), Error);
        CHECK_THROWS_AS(rhi.upload_buffer(buffer.get(), 9, data, 0), Error);
    }
}

TEST_CASE("RHICapabilities reports a texture-binding limit within the compile-time ceiling")
{
    NullRHI rhi;
    CHECK(rhi.capabilities().max_texture_bindings > 0);
    CHECK(rhi.capabilities().max_texture_bindings <= RHI_MAX_TEXTURE_BINDINGS);
    CHECK(DRAW_ITEM_MAX_TEXTURES == RHI_MAX_TEXTURE_BINDINGS);
    CHECK(TextureArrayPermutations::MAX_TEXTURES == RHI_MAX_TEXTURE_BINDINGS);
}

TEST_CASE("TransientAllocator hands out aligned, non-overlapping ranges per frame slot")
{
    NullRHI rhi;
    TransientAllocator allocator = TransientAllocator::create(rhi, 1000, RHIBufferUsage::Vertex);
    CHECK(allocator.capacity() == 1024);
    REQUIRE(allocator.region_count() >= 2);

    TransientAllocation a = allocator.allocate(0, 10, 16);
    TransientAllocation b = allocator.allocate(0, 4, 64);
    CHECK(a.offset == 0);
    CHECK(b.offset == 64);
    CHECK(b.data == a.data + 64);
    CHECK(allocator.used(0) == 68);
    CHECK(a.buffer == allocator.rhi_ptr());

    TransientAllocation other = allocator.allocate(1, 4);
    CHECK(other.offset == allocator.capacity());

    std::memset(a.data, 7, 10);
    CHECK(bytes_of(a.buffer)[9] == 7);

    allocator.reset(0);
    CHECK(allocator.used(0) == 0);
    CHECK(allocator.allocate(0, 4).offset == 0);
}

TEST_CASE("TransientAllocator throws when full or misused")
{
    NullRHI rhi;
    TransientAllocator allocator = TransientAllocator::create(rhi, 256, RHIBufferUsage::Vertex);
    CHECK_NOTHROW(allocator.allocate(0, 200));
    CHECK_THROWS_AS(allocator.allocate(0, 100), Error);
    CHECK_NOTHROW(allocator.allocate(0, 56, 1));
    CHECK_THROWS_AS(allocator.allocate(0, 1, 1), Error);
    CHECK_THROWS_AS(allocator.allocate(0, 1, 3), Error);
    CHECK_THROWS_AS(allocator.allocate(0, 1, 512), Error);
    CHECK_THROWS_AS(allocator.allocate(allocator.region_count(), 1), Error);
    CHECK_THROWS_AS(allocator.reset(allocator.region_count()), Error);
}

TEST_CASE("create_default_resources builds the shared quad indices")
{
    NullRHI rhi;
    DefaultResources defaults = create_default_resources(rhi);
    REQUIRE(defaults.quad_indices != nullptr);
    CHECK(defaults.quad_indices->memory() == RHIMemory::GpuOnly);
    CHECK(defaults.quad_indices->size() == QUAD_INDEX_COUNT * sizeof(uint16_t));

    const uint16_t* indices = reinterpret_cast<const uint16_t*>(bytes_of(defaults.quad_indices).data());
    const uint16_t expected_first[6] = { 0, 1, 2, 2, 3, 0 };
    for (uint32_t i = 0; i < 6; ++i)
    {
        CHECK(indices[i] == expected_first[i]);
        CHECK(indices[6 + i] == expected_first[i] + 4);
    }
    const uint32_t last = QUAD_INDEX_MAX_QUADS - 1;
    CHECK(indices[last * 6 + 4] == last * 4 + 3);
}

TEST_CASE("RHICommandList::copy_buffer records a copy, retains both buffers and executes it on submit")
{
    NullRHI rhi;
    RHIBufferPtr staging = rhi.create_buffer({ .size = 8, .usage = RHIBufferUsage::CopySource });
    RHIBufferPtr device = rhi.create_buffer({ .size = 8, .usage = RHIBufferUsage::Vertex | RHIBufferUsage::CopyDest, .memory = RHIMemory::GpuOnly });
    for (uint32_t i = 0; i < 8; ++i)
    {
        staging->map()[i] = static_cast<uint8_t>(10 + i);
    }

    RHICommandList list;
    list.copy_buffer(staging.get(), 2, device.get(), 4, 4);
    REQUIRE(list.size() == 1);
    const RHICopyBufferCommand* copy = command_cast<RHICopyBufferCommand>(*list.begin());
    REQUIRE(copy != nullptr);
    CHECK(copy->size() == 4);
    CHECK(copy->source_offset() == 2);
    CHECK(copy->destination_offset() == 4);
    CHECK(list.retained_count() == 2);

    rhi.submit(list);
    CHECK(rhi.last_submission() == std::vector<std::string_view>{ "CopyBuffer" });
    CHECK(bytes_of(device)[4] == 12);
    CHECK(bytes_of(device)[7] == 15);
    CHECK(bytes_of(device)[3] == 0);
}

TEST_CASE("RHICommandList::copy_buffer validates usage, ranges, aliasing and passes")
{
    NullRHI rhi;
    RHIBufferPtr source = rhi.create_buffer({ .size = 8, .usage = RHIBufferUsage::CopySource });
    RHIBufferPtr destination = rhi.create_buffer({ .size = 8, .usage = RHIBufferUsage::CopyDest });
    RHIBufferPtr plain = rhi.create_buffer({ .size = 8 });
    RHICommandList list;

    CHECK_THROWS_AS(list.copy_buffer(nullptr, 0, destination.get(), 0, 4), Error);
    CHECK_THROWS_AS(list.copy_buffer(source.get(), 0, nullptr, 0, 4), Error);
    CHECK_THROWS_AS(list.copy_buffer(plain.get(), 0, destination.get(), 0, 4), Error);
    CHECK_THROWS_AS(list.copy_buffer(source.get(), 0, plain.get(), 0, 4), Error);
    CHECK_THROWS_AS(list.copy_buffer(source.get(), 0, destination.get(), 0, 0), Error);
    CHECK_THROWS_AS(list.copy_buffer(source.get(), 6, destination.get(), 0, 4), Error);
    CHECK_THROWS_AS(list.copy_buffer(source.get(), 0, destination.get(), 6, 4), Error);
    CHECK_THROWS_AS(list.copy_buffer(source.get(), 9, destination.get(), 0, 1), Error);

    RHIBufferPtr both = rhi.create_buffer({ .size = 8, .usage = RHIBufferUsage::CopySource | RHIBufferUsage::CopyDest });
    CHECK_THROWS_AS(list.copy_buffer(both.get(), 0, both.get(), 4, 4), Error);
    CHECK(list.empty());

    RHIViewportPtr viewport = rhi.create_viewport({ .width = 4, .height = 4 });
    RHIRenderTargetPtr target = viewport->acquire_back_buffer();
    list.begin_pass(target.get());
    CHECK_THROWS_AS(list.copy_buffer(source.get(), 0, destination.get(), 0, 4), Error);
    list.end_pass();
    CHECK_NOTHROW(list.copy_buffer(source.get(), 0, destination.get(), 0, 8));
    list.clear();
}
