#pragma once

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Graphics/RHI/RHIFlags.h"
#include "Oryx/Graphics/RHI/RHIResource.h"

namespace oryx
{

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

// capacity * stride * regions as a buffer size; throws Error when it exceeds the 32-bit buffer limit. Any zero operand gives 0.
[[nodiscard]] inline uint32_t checked_buffer_bytes(size_t capacity, size_t stride, size_t regions)
{
    constexpr uint64_t limit = std::numeric_limits<uint32_t>::max();
    const uint64_t operands[3] = { capacity, stride, regions };
    uint64_t total = 1;
    for (const uint64_t operand : operands)
    {
        if (operand != 0 && total > limit / operand)
        {
            throw Error("RHI buffer size exceeds the 4 GiB limit", std::to_string(capacity) + " x " + std::to_string(stride) + " x " + std::to_string(regions));
        }
        total *= operand;
    }
    return static_cast<uint32_t>(total);
}

class RHIBuffer : public RHIResource
{
public:
    [[nodiscard]] uint32_t size() const { return m_size; }
    [[nodiscard]] RHIBufferUsage usage() const { return m_usage; }
    [[nodiscard]] RHIMemory memory() const { return m_memory; }

    // CpuToGpu buffers only; throws Error when the range exceeds the buffer.
    virtual void update(uint32_t offset, const uint8_t* data, uint32_t data_size) = 0;

    // CpuToGpu buffers only (throws Error otherwise): a pointer to the whole buffer that stays valid for its lifetime, so writers fill ranges in place.
    // The memory is write-combined on discrete GPUs: write sequentially, never read back, and keep ranges the GPU is still reading untouched.
    [[nodiscard]] virtual uint8_t* map() = 0;

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
