#include "oxpch.h"
#include "Oryx/Graphics/Resources/IndexBuffer.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

IndexBuffer::IndexBuffer(RHIBufferPtr buffer, IndexType type, uint32_t capacity, BufferMode mode, uint32_t region_count)
    : m_buffer(std::move(buffer))
    , m_capacity(capacity)
    , m_region_count(region_count)
    , m_type(type)
    , m_mode(mode)
{
    if (!m_buffer)
    {
        throw Error("IndexBuffer requires an RHI buffer");
    }
    if (m_capacity == 0 || m_region_count == 0)
    {
        throw Error("IndexBuffer capacity and region count must be non-zero");
    }
    if (checked_buffer_bytes(m_capacity, index_bytes(), m_region_count) > m_buffer->size())
    {
        throw Error("IndexBuffer RHI buffer is too small for its regions");
    }
}

IndexBuffer IndexBuffer::create(IRHI& rhi, IndexType type, uint32_t capacity, BufferMode mode)
{
    const uint32_t region_count = mode == BufferMode::Dynamic ? rhi.capabilities().frames_in_flight : 1;
    const uint32_t index_bytes = type == IndexType::U32 ? 4 : 2;
    RHIBufferPtr buffer = rhi.create_buffer({ .size = checked_buffer_bytes(capacity, index_bytes, region_count), .usage = RHIBufferUsage::Index, .memory = RHIMemory::CpuToGpu });
    return IndexBuffer(std::move(buffer), type, capacity, mode, region_count);
}

uint32_t IndexBuffer::offset(uint32_t frame_slot) const
{
    if (m_mode == BufferMode::Static)
    {
        return 0;
    }
    if (frame_slot >= m_region_count)
    {
        throw Error("IndexBuffer frame slot is out of range");
    }
    return frame_slot * m_capacity * index_bytes();
}

void IndexBuffer::set_data(uint32_t frame_slot, const uint16_t* data, uint32_t count)
{
    write(frame_slot, IndexType::U16, reinterpret_cast<const uint8_t*>(data), count);
}

void IndexBuffer::set_data(uint32_t frame_slot, const uint32_t* data, uint32_t count)
{
    write(frame_slot, IndexType::U32, reinterpret_cast<const uint8_t*>(data), count);
}

void IndexBuffer::write(uint32_t frame_slot, IndexType type, const uint8_t* data, uint32_t count)
{
    if (type != m_type)
    {
        throw Error("IndexBuffer data type does not match the index type");
    }
    if (count > m_capacity)
    {
        throw Error("IndexBuffer data exceeds the capacity");
    }
    if (m_mode == BufferMode::Static && m_written)
    {
        throw Error("A static IndexBuffer can only be written once");
    }
    m_buffer->update(offset(frame_slot), data, count * index_bytes());
    m_index_count = count;
    m_written = true;
}

} // namespace oryx
