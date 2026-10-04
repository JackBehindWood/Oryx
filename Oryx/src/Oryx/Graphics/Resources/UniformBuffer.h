#pragma once

#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIDeclarations.h"

namespace oryx
{

// Single region, no ring: per-frame data goes through set_constants.
class UniformBuffer
{
public:
    UniformBuffer(RHIBufferPtr buffer, uint32_t size);

    static UniformBuffer create(IRHI& rhi, uint32_t size);

    // Throws Error when sizeof(T) exceeds the buffer size.
    template<typename T>
    void set_data(const T& value)
    {
        write(reinterpret_cast<const uint8_t*>(&value), sizeof(T));
    }

    [[nodiscard]] RHIBuffer& rhi() const { return *m_buffer; }
    [[nodiscard]] const RHIBufferPtr& rhi_ptr() const { return m_buffer; }
    [[nodiscard]] uint32_t size() const { return m_size; }

private:
    void write(const uint8_t* data, size_t size);

    RHIBufferPtr m_buffer;
    uint32_t m_size;
};

} // namespace oryx
