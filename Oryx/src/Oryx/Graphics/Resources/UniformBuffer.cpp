#include "oxpch.h"
#include "Oryx/Graphics/Resources/UniformBuffer.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

namespace
{

uint32_t aligned_region_size(uint32_t size)
{
    return checked_buffer_bytes((static_cast<size_t>(size) + UniformBuffer::REGION_ALIGNMENT - 1) / UniformBuffer::REGION_ALIGNMENT, UniformBuffer::REGION_ALIGNMENT, 1);
}

} // namespace

UniformBuffer::UniformBuffer(RHIBufferPtr buffer, uint32_t size, uint32_t region_count)
    : m_buffer(std::move(buffer))
    , m_size(size)
    , m_region_count(region_count)
    , m_region_size(aligned_region_size(size))
{
    if (!m_buffer)
    {
        throw Error("UniformBuffer requires an RHI buffer");
    }
    if (m_size == 0 || m_region_count == 0)
    {
        throw Error("UniformBuffer size and region count must be non-zero");
    }
    if (checked_buffer_bytes(m_region_size, 1, m_region_count) > m_buffer->size())
    {
        throw Error("UniformBuffer RHI buffer is too small for its regions");
    }
}

UniformBuffer UniformBuffer::create(IRHI& rhi, uint32_t size)
{
    const uint32_t region_count = rhi.capabilities().frames_in_flight;
    const uint32_t bytes = checked_buffer_bytes(aligned_region_size(size), 1, region_count);
    return UniformBuffer(rhi.create_buffer({ .size = bytes, .usage = RHIBufferUsage::Uniform, .memory = RHIMemory::CpuToGpu }), size, region_count);
}

uint32_t UniformBuffer::offset(uint32_t frame_slot) const
{
    if (frame_slot >= m_region_count)
    {
        throw Error("UniformBuffer frame slot is out of range");
    }
    return frame_slot * m_region_size;
}

void UniformBuffer::write(uint32_t frame_slot, const uint8_t* data, size_t size)
{
    if (size > m_size)
    {
        throw Error("UniformBuffer data exceeds the buffer size");
    }
    m_buffer->update(offset(frame_slot), data, static_cast<uint32_t>(size));
}

} // namespace oryx
