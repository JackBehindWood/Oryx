#include "oxpch.h"
#include "Oryx/Debug/MemoryTracker.h"

namespace oryx
{

MemoryStats MemoryTracker::snapshot()
{
    return detail::g_census.snapshot();
}

void MemoryTracker::reset_peak()
{
    detail::g_census.reset_peak();
}

MemoryStats MemoryTracker::begin_measurement()
{
    return detail::g_census.begin_measurement();
}

// Only the replacement records, and every executable allocates during static init.
bool MemoryTracker::is_installed()
{
    return detail::g_census.snapshot().allocation_count > 0;
}

} // namespace oryx
