#include "oxpch.h"
#include "MetalResources.h"

#include "Oryx/Core/Error.h"

namespace oryx::metal
{

MetalBuffer::MetalBuffer(const RHIBufferDesc& desc, NS::SharedPtr<MTL::Buffer> buffer)
    : RHIBuffer(desc)
    , m_buffer(std::move(buffer))
{
}

void MetalBuffer::update(uint32_t offset, const uint8_t* data, uint32_t data_size)
{
    if (memory() != RHIMemory::CpuToGpu)
    {
        throw Error("RHI buffer update requires CpuToGpu memory");
    }
    if (offset > size() || data_size > size() - offset)
    {
        throw Error("RHI buffer update is out of range");
    }
    if (data_size > 0)
    {
        std::memcpy(static_cast<uint8_t*>(m_buffer->contents()) + offset, data, data_size);
    }
}

} // namespace oryx::metal
