#pragma once

#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIDeclarations.h"

namespace oryx
{

// One region per frame in flight, each a multiple of REGION_ALIGNMENT; write the slot of the frame being recorded and bind the range at offset(frame_slot).
class UniformBuffer
{
public:
    static constexpr uint32_t REGION_ALIGNMENT = 256;

    UniformBuffer(RHIBufferPtr buffer, uint32_t size, uint32_t region_count);

    static UniformBuffer create(IRHI& rhi, uint32_t size);

    // Throws Error when sizeof(T) exceeds the buffer size or frame_slot is out of range.
    template<typename T>
    void set_data(uint32_t frame_slot, const T& value)
    {
        write(frame_slot, reinterpret_cast<const uint8_t*>(&value), sizeof(T));
    }

    [[nodiscard]] RHIBuffer& rhi() const { return *m_buffer; }
    [[nodiscard]] const RHIBufferPtr& rhi_ptr() const { return m_buffer; }
    [[nodiscard]] uint32_t size() const { return m_size; }
    [[nodiscard]] uint32_t region_count() const { return m_region_count; }
    [[nodiscard]] uint32_t region_size() const { return m_region_size; }
    [[nodiscard]] uint32_t offset(uint32_t frame_slot) const;

private:
    void write(uint32_t frame_slot, const uint8_t* data, size_t size);

    RHIBufferPtr m_buffer;
    uint32_t m_size;
    uint32_t m_region_count;
    uint32_t m_region_size;
};

} // namespace oryx
