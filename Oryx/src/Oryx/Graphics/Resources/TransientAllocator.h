#pragma once

#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIDeclarations.h"

namespace oryx
{

struct TransientAllocation
{
    RHIBufferPtr buffer;
    uint32_t offset = 0;
    uint8_t* data = nullptr;
};

// Per-frame linear sub-allocator over one persistently mapped upload buffer, one region per frame in flight; callers write `data` in place and draw from `buffer` at `offset`.
class TransientAllocator
{
public:
    TransientAllocator(RHIBufferPtr buffer, uint32_t capacity, uint32_t region_count);

    // `capacity` is bytes per frame slot, rounded up to a multiple of REGION_ALIGNMENT; CopySource is always added so ranges can be staged into GpuOnly buffers (RHICommandList::copy_buffer).
    static TransientAllocator create(IRHI& rhi, uint32_t capacity, RHIBufferUsage usage);

    // Throws Error when the region is full, `alignment` is not a power of two no larger than REGION_ALIGNMENT, or frame_slot is out of range.
    [[nodiscard]] TransientAllocation allocate(uint32_t frame_slot, uint32_t size, uint32_t alignment = 16);
    // Frees the whole region; call at the start of the frame that reuses the slot.
    void reset(uint32_t frame_slot);

    [[nodiscard]] const RHIBufferPtr& rhi_ptr() const { return m_buffer; }
    [[nodiscard]] uint32_t capacity() const { return m_capacity; }
    [[nodiscard]] uint32_t region_count() const { return m_region_count; }
    [[nodiscard]] uint32_t used(uint32_t frame_slot) const;

    static constexpr uint32_t REGION_ALIGNMENT = 256;

private:
    RHIBufferPtr m_buffer;
    uint8_t* m_mapped;
    uint32_t m_capacity;
    uint32_t m_region_count;
    std::vector<uint32_t> m_cursors;
};

} // namespace oryx
