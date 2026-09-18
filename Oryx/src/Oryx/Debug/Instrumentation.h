#pragma once

#include "Oryx/Core/Base.h"

#include <chrono>

namespace oryx
{

struct ProfileSample
{
    int64_t call_count = 0;
    double total_milliseconds = 0.0;
};

class Instrumentation
{
public:
    static void record(const std::string& name, double milliseconds);
    static void reset();
    static std::unordered_map<std::string, ProfileSample> results();
};

class ScopeTimer
{
public:
    explicit ScopeTimer(std::string name);
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    std::string m_name;
    std::chrono::high_resolution_clock::time_point m_start;
};

} // namespace oryx

#define OX_PROFILE_SCOPE(name) ::oryx::ScopeTimer OX_CONCAT(ox_scope_timer_, __LINE__)(name)
