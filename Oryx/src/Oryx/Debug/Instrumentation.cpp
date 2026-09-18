#include "oxpch.h"
#include "Oryx/Debug/Instrumentation.h"

namespace oryx
{

namespace
{

std::unordered_map<std::string, ProfileSample>& registry()
{
    static std::unordered_map<std::string, ProfileSample> instance;
    return instance;
}

} // namespace

void Instrumentation::record(const std::string& name, double milliseconds)
{
    ProfileSample& sample = registry()[name];
    ++sample.call_count;
    sample.total_milliseconds += milliseconds;
}

void Instrumentation::reset()
{
    registry().clear();
}

std::unordered_map<std::string, ProfileSample> Instrumentation::results()
{
    return registry();
}

ScopeTimer::ScopeTimer(std::string name)
    : m_name(std::move(name))
    , m_start(std::chrono::high_resolution_clock::now())
{
}

ScopeTimer::~ScopeTimer()
{
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - m_start).count();
    Instrumentation::record(m_name, ms);
}

} // namespace oryx
