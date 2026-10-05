#pragma once

#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Graphics/Resources/BufferMode.h"
#include "Oryx/Graphics/RHI/RHIVertexDeclaration.h"

namespace oryx
{

// Dynamic buffers hold `region_count` regions (one per frame in flight); callers write the region of the frame being recorded.
class VertexBuffer
{
public:
    VertexBuffer(RHIBufferPtr buffer, RHIVertexDeclaration declaration, uint32_t capacity, BufferMode mode, uint32_t region_count);

    static VertexBuffer create(IRHI& rhi, const RHIVertexDeclaration& declaration, uint32_t capacity, BufferMode mode);

    // Throws Error unless sizeof(T) == declaration.stride(), count <= capacity and (Dynamic) frame_slot < region_count; a Static buffer is written once.
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

    // Writes after the vertices already appended to the frame slot's region and returns the index of the first one (the draw's base_vertex).
    // Throws Error unless sizeof(T) == declaration.stride(), the region has room for `count` more and the buffer is Dynamic; reset(frame_slot) starts the region over.
    template<typename T>
    uint32_t append(uint32_t frame_slot, const T* data, uint32_t count)
    {
        return append_bytes(frame_slot, reinterpret_cast<const uint8_t*>(data), count, sizeof(T));
    }

    template<typename T, size_t N>
    uint32_t append(uint32_t frame_slot, const T (&data)[N])
    {
        return append(frame_slot, data, static_cast<uint32_t>(N));
    }

    void reset(uint32_t frame_slot);
    [[nodiscard]] uint32_t appended(uint32_t frame_slot) const;

    [[nodiscard]] RHIBuffer& rhi() const { return *m_buffer; }
    [[nodiscard]] const RHIBufferPtr& rhi_ptr() const { return m_buffer; }
    [[nodiscard]] const RHIVertexDeclaration& declaration() const { return m_declaration; }
    [[nodiscard]] BufferMode mode() const { return m_mode; }
    [[nodiscard]] uint32_t capacity() const { return m_capacity; }
    [[nodiscard]] uint32_t region_count() const { return m_region_count; }
    [[nodiscard]] uint32_t vertex_count() const { return m_vertex_count; }
    [[nodiscard]] uint32_t offset(uint32_t frame_slot) const;

private:
    void write(uint32_t frame_slot, const uint8_t* data, uint32_t count, size_t element_size);
    uint32_t append_bytes(uint32_t frame_slot, const uint8_t* data, uint32_t count, size_t element_size);

    RHIBufferPtr m_buffer;
    RHIVertexDeclaration m_declaration;
    uint32_t m_stride = 0;
    uint32_t m_capacity;
    uint32_t m_region_count;
    uint32_t m_vertex_count = 0;
    std::vector<uint32_t> m_cursors;
    BufferMode m_mode;
    bool m_written = false;
};

} // namespace oryx
