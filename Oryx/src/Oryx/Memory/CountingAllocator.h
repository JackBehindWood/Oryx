#pragma once

#include "Oryx/Memory/IAllocator.h"
#include "Oryx/Memory/MemoryStats.h"

namespace oryx
{

class CountingAllocator final : public IAllocator
{
public:
    explicit CountingAllocator(IAllocator& upstream)
        : m_upstream(upstream)
    {
    }

    [[nodiscard]] void* allocate(size_t size, size_t alignment) override
    {
        void* pointer = m_upstream.allocate(size, alignment);
        m_counters.record_allocation(size);
        return pointer;
    }

    void deallocate(void* pointer, size_t size, size_t alignment) noexcept override
    {
        m_counters.record_deallocation(size);
        m_upstream.deallocate(pointer, size, alignment);
    }

    [[nodiscard]] MemoryCounters& counters() { return m_counters; }
    [[nodiscard]] const MemoryCounters& counters() const { return m_counters; }

private:
    IAllocator& m_upstream;
    MemoryCounters m_counters;
};

} // namespace oryx
