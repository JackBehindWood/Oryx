#include "oxpch.h"
#include "Oryx/Debug/Instrumentation.h"

namespace oryx
{

namespace
{

std::unordered_map<std::string_view, ProfileSample>& registry()
{
    static std::unordered_map<std::string_view, ProfileSample> instance;
    return instance;
}

} // namespace

void Instrumentation::record(std::string_view name, double milliseconds, const MemoryStats& memory)
{
    ProfileSample& sample = registry()[name];
    ++sample.call_count;
    sample.total_milliseconds += milliseconds;
    sample.min_milliseconds = std::min(sample.min_milliseconds, milliseconds);
    sample.max_milliseconds = std::max(sample.max_milliseconds, milliseconds);
    sample.allocation_count += memory.allocation_count;
    sample.bytes_allocated += memory.bytes_allocated;
}

void Instrumentation::reset()
{
    registry().clear();
}

const std::unordered_map<std::string_view, ProfileSample>& Instrumentation::results()
{
    return registry();
}

ScopeTimer::ScopeTimer(const char* name)
    : m_name(name)
#ifdef OX_ENABLE_MEMORY_TRACKING
    , m_memory_before(MemoryTracker::snapshot())
#endif
    , m_start(std::chrono::steady_clock::now())
{
}

ScopeTimer::~ScopeTimer()
{
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    MemoryStats memory;
#ifdef OX_ENABLE_MEMORY_TRACKING
    memory = memory_delta(m_memory_before, MemoryTracker::snapshot());
#endif
    double ms = std::chrono::duration<double, std::milli>(end - m_start).count();
    Instrumentation::record(m_name, ms, memory);
}

} // namespace oryx
