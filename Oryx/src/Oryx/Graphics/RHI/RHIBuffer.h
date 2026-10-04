#pragma once

#include "Oryx/Graphics/RHI/RHIFlags.h"
#include "Oryx/Graphics/RHI/RHIResource.h"

namespace oryx
{

enum class RHIBufferUsage : uint8_t
{
    Vertex = BIT(0),
    Index = BIT(1),
    Uniform = BIT(2),
    Storage = BIT(3)
};

template<>
inline constexpr bool rhi_flags_enum<RHIBufferUsage> = true;

enum class RHIMemory : uint8_t
{
    CpuToGpu,
    GpuOnly
};

// Sizes and offsets are 32-bit: a buffer is limited to 4 GiB. initial_data is only read during creation.
struct RHIBufferDesc
{
    uint32_t size = 0;
    RHIBufferUsage usage = RHIBufferUsage::Vertex;
    RHIMemory memory = RHIMemory::CpuToGpu;
    const uint8_t* initial_data = nullptr;
    uint32_t initial_data_size = 0;
    const char* name = nullptr;
};

class RHIBuffer : public RHIResource
{
public:
    [[nodiscard]] uint32_t size() const { return m_size; }
    [[nodiscard]] RHIBufferUsage usage() const { return m_usage; }
    [[nodiscard]] RHIMemory memory() const { return m_memory; }

    // CpuToGpu buffers only; throws Error when the range exceeds the buffer.
    virtual void update(uint32_t offset, const uint8_t* data, uint32_t data_size) = 0;

protected:
    explicit RHIBuffer(const RHIBufferDesc& desc)
        : RHIResource()
        , m_size(desc.size)
        , m_usage(desc.usage)
        , m_memory(desc.memory)
    {
    }

private:
    uint32_t m_size;
    RHIBufferUsage m_usage;
    RHIMemory m_memory;
};

using RHIBufferPtr = Ref<RHIBuffer>;

} // namespace oryx
