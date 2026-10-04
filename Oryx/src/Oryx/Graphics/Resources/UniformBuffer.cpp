#include "oxpch.h"
#include "Oryx/Graphics/Resources/UniformBuffer.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

UniformBuffer::UniformBuffer(RHIBufferPtr buffer, uint32_t size)
    : m_buffer(std::move(buffer))
    , m_size(size)
{
    if (!m_buffer)
    {
        throw Error("UniformBuffer requires an RHI buffer");
    }
    if (m_size == 0 || m_size > m_buffer->size())
    {
        throw Error("UniformBuffer size must be non-zero and fit the RHI buffer");
    }
}

UniformBuffer UniformBuffer::create(IRHI& rhi, uint32_t size)
{
    return UniformBuffer(rhi.create_buffer({ .size = size, .usage = RHIBufferUsage::Uniform, .memory = RHIMemory::CpuToGpu }), size);
}

void UniformBuffer::write(const uint8_t* data, size_t size)
{
    if (size > m_size)
    {
        throw Error("UniformBuffer data exceeds the buffer size");
    }
    m_buffer->update(0, data, static_cast<uint32_t>(size));
}

} // namespace oryx
