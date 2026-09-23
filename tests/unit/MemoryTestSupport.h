#pragma once

#include "Oryx/Debug/MemoryTracker.h"
#include "Oryx/Memory/DefaultAllocator.h"

namespace oryx::test
{

// Allocation budgets count both sources: the heap census and Oryx's default allocator (its chunk refills reach the census too).
inline MemoryStats all_allocations()
{
    MemoryStats total = MemoryTracker::snapshot();
    MemoryStats pool = default_allocator_stats();
    total.allocation_count += pool.allocation_count;
    total.deallocation_count += pool.deallocation_count;
    total.bytes_allocated += pool.bytes_allocated;
    total.bytes_freed += pool.bytes_freed;
    total.live_bytes += pool.live_bytes;
    // The two peaks need not coincide, so their sum is an upper bound.
    total.peak_live_bytes += pool.peak_live_bytes;
    return total;
}

} // namespace oryx::test
