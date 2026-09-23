#include "oxpch.h"
#include "Oryx/Memory/MemoryStats.h"

namespace oryx
{

MemoryStats memory_delta(const MemoryStats& before, const MemoryStats& after)
{
    MemoryStats delta;
    delta.allocation_count = after.allocation_count - before.allocation_count;
    delta.deallocation_count = after.deallocation_count - before.deallocation_count;
    delta.bytes_allocated = after.bytes_allocated - before.bytes_allocated;
    delta.bytes_freed = after.bytes_freed - before.bytes_freed;
    delta.live_bytes = after.live_bytes - before.live_bytes;
    delta.peak_live_bytes = std::max<int64_t>(after.peak_live_bytes - before.live_bytes, 0);
    return delta;
}

MemoryStats MemoryCounters::snapshot() const
{
    MemoryStats stats;
    stats.allocation_count = m_allocation_count.load(std::memory_order_relaxed);
    stats.deallocation_count = m_deallocation_count.load(std::memory_order_relaxed);
    stats.bytes_allocated = m_bytes_allocated.load(std::memory_order_relaxed);
    stats.bytes_freed = m_bytes_freed.load(std::memory_order_relaxed);
    stats.live_bytes = m_live_bytes.load(std::memory_order_relaxed);
    stats.peak_live_bytes = m_peak_live_bytes.load(std::memory_order_relaxed);
    return stats;
}

void MemoryCounters::reset_peak()
{
    m_peak_live_bytes.store(m_live_bytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

MemoryStats MemoryCounters::begin_measurement()
{
    reset_peak();
    return snapshot();
}

} // namespace oryx
