#include "oxpch.h"
#include "Oryx/Graphics/Resources/VertexBuffer.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

VertexBuffer::VertexBuffer(RHIBufferPtr buffer, VertexLayout layout, uint32_t capacity, BufferMode mode, uint32_t region_count)
    : m_buffer(std::move(buffer))
    , m_layout(std::move(layout))
    , m_capacity(capacity)
    , m_region_count(region_count)
    , m_cursors(region_count, 0)
    , m_mode(mode)
{
    if (!m_buffer)
    {
        throw Error("VertexBuffer requires an RHI buffer");
    }
    if (m_layout.stride == 0 || m_capacity == 0 || m_region_count == 0)
    {
        throw Error("VertexBuffer stride, capacity and region count must be non-zero");
    }
    if (static_cast<uint64_t>(m_capacity) * m_layout.stride * m_region_count > m_buffer->size())
    {
        throw Error("VertexBuffer RHI buffer is too small for its regions");
    }
}

VertexBuffer VertexBuffer::create(IRHI& rhi, const VertexLayout& layout, uint32_t capacity, BufferMode mode)
{
    const uint32_t region_count = mode == BufferMode::Dynamic ? rhi.capabilities().frames_in_flight : 1;
    RHIBufferPtr buffer = rhi.create_buffer({ .size = capacity * layout.stride * region_count, .usage = RHIBufferUsage::Vertex, .memory = RHIMemory::CpuToGpu });
    return VertexBuffer(std::move(buffer), layout, capacity, mode, region_count);
}

uint32_t VertexBuffer::offset(uint32_t frame_slot) const
{
    if (m_mode == BufferMode::Static)
    {
        return 0;
    }
    if (frame_slot >= m_region_count)
    {
        throw Error("VertexBuffer frame slot is out of range");
    }
    return frame_slot * m_capacity * m_layout.stride;
}

void VertexBuffer::write(uint32_t frame_slot, const uint8_t* data, uint32_t count, size_t element_size)
{
    if (element_size != m_layout.stride)
    {
        throw Error("VertexBuffer data type size does not match the layout stride");
    }
    if (count > m_capacity)
    {
        throw Error("VertexBuffer data exceeds the capacity");
    }
    if (m_mode == BufferMode::Static && m_written)
    {
        throw Error("A static VertexBuffer can only be written once");
    }
    m_buffer->update(offset(frame_slot), data, count * m_layout.stride);
    m_vertex_count = count;
    m_written = true;
}

uint32_t VertexBuffer::append_bytes(uint32_t frame_slot, const uint8_t* data, uint32_t count, size_t element_size)
{
    if (element_size != m_layout.stride)
    {
        throw Error("VertexBuffer data type size does not match the layout stride");
    }
    if (m_mode != BufferMode::Dynamic)
    {
        throw Error("Only a Dynamic VertexBuffer can be appended to");
    }
    if (frame_slot >= m_region_count)
    {
        throw Error("VertexBuffer frame slot is out of range");
    }
    const uint32_t base = m_cursors[frame_slot];
    if (count > m_capacity - base)
    {
        throw Error("VertexBuffer append exceeds the capacity", "capacity is " + std::to_string(m_capacity) + " vertices, " + std::to_string(base) + " already appended");
    }
    m_buffer->update(offset(frame_slot) + base * m_layout.stride, data, count * m_layout.stride);
    m_cursors[frame_slot] = base + count;
    m_vertex_count = base + count;
    return base;
}

void VertexBuffer::reset(uint32_t frame_slot)
{
    if (frame_slot >= m_region_count)
    {
        throw Error("VertexBuffer frame slot is out of range");
    }
    m_cursors[frame_slot] = 0;
}

uint32_t VertexBuffer::appended(uint32_t frame_slot) const
{
    if (frame_slot >= m_region_count)
    {
        throw Error("VertexBuffer frame slot is out of range");
    }
    return m_cursors[frame_slot];
}

} // namespace oryx
