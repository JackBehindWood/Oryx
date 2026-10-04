#pragma once

#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Graphics/Resources/BufferMode.h"
#include "Oryx/Graphics/Resources/VertexLayout.h"

namespace oryx
{

// Dynamic buffers hold `region_count` regions (one per frame in flight); callers write the region of the frame being recorded.
class VertexBuffer
{
public:
    VertexBuffer(RHIBufferPtr buffer, VertexLayout layout, uint32_t capacity, BufferMode mode, uint32_t region_count);

    static VertexBuffer create(IRHI& rhi, const VertexLayout& layout, uint32_t capacity, BufferMode mode);

    // Throws Error unless sizeof(T) == layout.stride, count <= capacity and (Dynamic) frame_slot < region_count; a Static buffer is written once.
    template<typename T>
    void set_data(uint32_t frame_slot, const T* data, uint32_t count)
    {
        write(frame_slot, reinterpret_cast<const uint8_t*>(data), count, sizeof(T));
    }

    template<typename T, size_t N>
    void set_data(uint32_t frame_slot, const T (&data)[N])
    {
        set_data(frame_slot, data, static_cast<uint32_t>(N));
    }

    [[nodiscard]] RHIBuffer& rhi() const { return *m_buffer; }
    [[nodiscard]] const RHIBufferPtr& rhi_ptr() const { return m_buffer; }
    [[nodiscard]] const VertexLayout& layout() const { return m_layout; }
    [[nodiscard]] BufferMode mode() const { return m_mode; }
    [[nodiscard]] uint32_t capacity() const { return m_capacity; }
    [[nodiscard]] uint32_t region_count() const { return m_region_count; }
    [[nodiscard]] uint32_t vertex_count() const { return m_vertex_count; }
    [[nodiscard]] uint32_t offset(uint32_t frame_slot) const;

private:
    void write(uint32_t frame_slot, const uint8_t* data, uint32_t count, size_t element_size);

    RHIBufferPtr m_buffer;
    VertexLayout m_layout;
    uint32_t m_capacity;
    uint32_t m_region_count;
    uint32_t m_vertex_count = 0;
    BufferMode m_mode;
    bool m_written = false;
};

} // namespace oryx
