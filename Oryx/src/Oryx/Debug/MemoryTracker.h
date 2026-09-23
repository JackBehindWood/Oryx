#pragma once

#include "Oryx/Memory/MemoryStats.h"

namespace oryx
{

// The whole-program allocation census, fed by the global operator new replacement that Oryx executables link.
class MemoryTracker
{
public:
    static MemoryStats snapshot();
    static void reset_peak();
    static MemoryStats begin_measurement();
    // False where no executable linked the replacement (Oryx/backends/Program), e.g. the standalone oryx module.
    static bool is_installed();
};

namespace detail
{

inline constinit MemoryCounters g_census;

inline void record_allocation(size_t size) { g_census.record_allocation(size); }
inline void record_deallocation(size_t size) { g_census.record_deallocation(size); }

} // namespace detail

} // namespace oryx
