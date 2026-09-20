#pragma once

namespace oryx
{

struct MemoryStats
{
    uint64_t allocation_count = 0;
    uint64_t deallocation_count = 0;
    uint64_t bytes_allocated = 0;
    uint64_t bytes_freed = 0;
    int64_t live_bytes = 0;
    int64_t peak_live_bytes = 0;
};

class MemoryTracker
{
public:
    static MemoryStats snapshot();
    static void reset_peak();
    static MemoryStats begin_measurement();
};

// peak_live_bytes is the peak growth above before.live_bytes, so take `before` from begin_measurement().
MemoryStats memory_delta(const MemoryStats& before, const MemoryStats& after);

} // namespace oryx
