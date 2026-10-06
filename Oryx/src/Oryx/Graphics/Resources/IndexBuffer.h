#pragma once

#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Graphics/Resources/BufferMode.h"

namespace oryx
{

enum class IndexType : uint8_t
{
    U16,
    U32
};

class IndexBuffer
{
public:
    IndexBuffer(RHIBufferPtr buffer, IndexType type, uint32_t capacity, BufferMode mode, uint32_t region_count);

    static IndexBuffer create(IRHI& rhi, IndexType type, uint32_t capacity, BufferMode mode);

    // Throws Error unless the overload matches the index type, count <= capacity and (Dynamic) frame_slot < region_count; a Static buffer is written once.
    void set_data(uint32_t frame_slot, const uint16_t* data, uint32_t count);
    void set_data(uint32_t frame_slot, const uint32_t* data, uint32_t count);

    template<size_t N>
    void set_data(uint32_t frame_slot, const uint16_t (&data)[N])
    {
        set_data(frame_slot, data, static_cast<uint32_t>(N));
    }

    template<size_t N>
    void set_data(uint32_t frame_slot, const uint32_t (&data)[N])
    {
        set_data(frame_slot, data, static_cast<uint32_t>(N));
    }

    [[nodiscard]] RHIBuffer& rhi() const { return *m_buffer; }
    [[nodiscard]] const RHIBufferPtr& rhi_ptr() const { return m_buffer; }
    [[nodiscard]] IndexType type() const { return m_type; }
    [[nodiscard]] bool index32() const { return m_type == IndexType::U32; }
    [[nodiscard]] BufferMode mode() const { return m_mode; }
    [[nodiscard]] uint32_t capacity() const { return m_capacity; }
    [[nodiscard]] uint32_t region_count() const { return m_region_count; }
    [[nodiscard]] uint32_t index_count() const { return m_index_count; }
    [[nodiscard]] uint32_t offset(uint32_t frame_slot) const;

private:
    void write(uint32_t frame_slot, IndexType type, const uint8_t* data, uint32_t count);
    [[nodiscard]] uint32_t index_bytes() const { return index32() ? 4 : 2; }

    RHIBufferPtr m_buffer;
    uint32_t m_capacity;
    uint32_t m_region_count;
    uint32_t m_index_count = 0;
    IndexType m_type;
    BufferMode m_mode;
    bool m_written = false;
};

} // namespace oryx
