#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Memory/MemoryStats.h"

#include <chrono>
#include <limits>

namespace oryx
{

struct ProfileSample
{
    int64_t call_count = 0;
    double total_milliseconds = 0.0;
    double min_milliseconds = std::numeric_limits<double>::max();
    double max_milliseconds = 0.0;
    uint64_t allocation_count = 0;
    uint64_t bytes_allocated = 0;
};

class Instrumentation
{
public:
    static void record(std::string_view name, double milliseconds, const MemoryStats& memory);
    static void reset();
    static const std::unordered_map<std::string_view, ProfileSample>& results();

    static constexpr bool is_enabled()
    {
#ifdef OX_ENABLE_PROFILING
        return true;
#else
        return false;
#endif
    }

    static constexpr bool tracks_memory()
    {
#ifdef OX_ENABLE_MEMORY_TRACKING
        return true;
#else
        return false;
#endif
    }
};

class ScopeTimer
{
public:
    explicit ScopeTimer(const char* name);
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    const char* m_name;
#ifdef OX_ENABLE_MEMORY_TRACKING
    MemoryStats m_memory_before;
#endif
    std::chrono::steady_clock::time_point m_start;
};

} // namespace oryx

// Same opt-in mechanism as OX_ENABLE_ASSERTS (Base.h); `name` must be a string literal since the registry stores views.
#ifdef OX_ENABLE_PROFILING
    #define OX_PROFILE_SCOPE(name) ::oryx::ScopeTimer OX_CONCAT(ox_scope_timer_, __LINE__)(name)
#else
    #define OX_PROFILE_SCOPE(name)
#endif
