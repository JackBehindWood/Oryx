#pragma once

#include <chrono>

namespace oryx
{

// A plain stopwatch for timing a whole benchmark run (e.g. a batch of
// matches). Always compiled in - this measures the benchmark's own work,
// not a per-call-site hot loop, so it isn't gated by OX_ENABLE_PROFILING
// the way Debug/Instrumentation.h's ScopeTimer is.
class Timer
{
public:
    void start() { m_start = std::chrono::high_resolution_clock::now(); }
    void stop() { m_end = std::chrono::high_resolution_clock::now(); }

    [[nodiscard]] double elapsed_seconds() const
    {
        return std::chrono::duration<double>(m_end - m_start).count();
    }

private:
    std::chrono::high_resolution_clock::time_point m_start{};
    std::chrono::high_resolution_clock::time_point m_end{};
};

} // namespace oryx
