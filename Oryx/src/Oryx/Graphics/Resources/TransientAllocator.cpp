#include "oxpch.h"
#include "Oryx/Graphics/Resources/TransientAllocator.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

TransientAllocator::TransientAllocator(RHIBufferPtr buffer, uint32_t capacity, uint32_t region_count)
    : m_buffer(std::move(buffer))
    , m_mapped(nullptr)
    , m_capacity(capacity)
    , m_region_count(region_count)
    , m_cursors(region_count, 0)
{
    if (!m_buffer)
    {
        throw Error("TransientAllocator requires an RHI buffer");
    }
    if (m_capacity == 0 || m_region_count == 0 || m_capacity % REGION_ALIGNMENT != 0)
    {
        throw Error("TransientAllocator capacity must be a non-zero multiple of the region alignment");
    }
    if (static_cast<uint64_t>(m_capacity) * m_region_count > m_buffer->size())
    {
        throw Error("TransientAllocator RHI buffer is too small for its regions");
    }
    m_mapped = m_buffer->map();
}

TransientAllocator TransientAllocator::create(IRHI& rhi, uint32_t capacity, RHIBufferUsage usage)
{
    const uint32_t region_count = rhi.capabilities().frames_in_flight;
    const uint32_t aligned = checked_buffer_bytes((static_cast<size_t>(capacity) + REGION_ALIGNMENT - 1) / REGION_ALIGNMENT, REGION_ALIGNMENT, 1);
    const uint32_t size = checked_buffer_bytes(aligned, 1, region_count);
    RHIBufferPtr buffer = rhi.create_buffer({ .size = size, .usage = usage | RHIBufferUsage::CopySource, .memory = RHIMemory::CpuToGpu, .name = "TransientAllocator" });
    return TransientAllocator(std::move(buffer), aligned, region_count);
}

TransientAllocation TransientAllocator::allocate(uint32_t frame_slot, uint32_t size, uint32_t alignment)
{
    if (frame_slot >= m_region_count)
    {
        throw Error("TransientAllocator frame slot is out of range");
    }
    if (alignment == 0 || (alignment & (alignment - 1)) != 0 || alignment > REGION_ALIGNMENT)
    {
        throw Error("TransientAllocator alignment must be a power of two up to the region alignment");
    }
    const uint32_t start = (m_cursors[frame_slot] + alignment - 1) & ~(alignment - 1);
    if (start > m_capacity || size > m_capacity - start)
    {
        throw Error("TransientAllocator region is full", std::to_string(size) + " bytes requested, " + std::to_string(m_capacity - std::min(start, m_capacity)) + " left");
    }
    m_cursors[frame_slot] = start + size;
    const uint32_t offset = frame_slot * m_capacity + start;
    return { m_buffer, offset, m_mapped + offset };
}

void TransientAllocator::reset(uint32_t frame_slot)
{
    if (frame_slot >= m_region_count)
    {
        throw Error("TransientAllocator frame slot is out of range");
    }
    m_cursors[frame_slot] = 0;
}

uint32_t TransientAllocator::used(uint32_t frame_slot) const
{
    if (frame_slot >= m_region_count)
    {
        throw Error("TransientAllocator frame slot is out of range");
    }
    return m_cursors[frame_slot];
}

} // namespace oryx
