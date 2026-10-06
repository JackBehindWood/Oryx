#include "oxpch.h"
#include "Oryx/Graphics/Resources/VertexBuffer.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

VertexBuffer::VertexBuffer(RHIBufferPtr buffer, RHIVertexDeclaration declaration, uint32_t capacity, BufferMode mode, uint32_t region_count)
    : m_buffer(std::move(buffer))
    , m_declaration(std::move(declaration))
    , m_stride(m_declaration.stride())
    , m_capacity(capacity)
    , m_region_count(region_count)
    , m_mode(mode)
{
    if (!m_buffer)
    {
        throw Error("VertexBuffer requires an RHI buffer");
    }
    if (m_stride == 0 || m_capacity == 0 || m_region_count == 0)
    {
        throw Error("VertexBuffer stride, capacity and region count must be non-zero");
    }
    if (checked_buffer_bytes(m_capacity, m_stride, m_region_count) > m_buffer->size())
    {
        throw Error("VertexBuffer RHI buffer is too small for its regions");
    }
}

VertexBuffer VertexBuffer::create(IRHI& rhi, const RHIVertexDeclaration& declaration, uint32_t capacity, BufferMode mode)
{
    const uint32_t region_count = mode == BufferMode::Dynamic ? rhi.capabilities().frames_in_flight : 1;
    RHIBufferPtr buffer = rhi.create_buffer({ .size = checked_buffer_bytes(capacity, declaration.stride(), region_count), .usage = RHIBufferUsage::Vertex, .memory = RHIMemory::CpuToGpu });
    return VertexBuffer(std::move(buffer), declaration, capacity, mode, region_count);
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
    return frame_slot * m_capacity * m_stride;
}

void VertexBuffer::write(uint32_t frame_slot, const uint8_t* data, uint32_t count, size_t element_size)
{
    if (element_size != m_stride)
    {
        throw Error("VertexBuffer data type size does not match the declaration stride");
    }
    if (count > m_capacity)
    {
        throw Error("VertexBuffer data exceeds the capacity");
    }
    if (m_mode == BufferMode::Static && m_written)
    {
        throw Error("A static VertexBuffer can only be written once");
    }
    m_buffer->update(offset(frame_slot), data, count * m_stride);
    m_vertex_count = count;
    m_written = true;
}

} // namespace oryx
