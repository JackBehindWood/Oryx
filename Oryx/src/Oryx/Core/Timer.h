#pragma once

#include <chrono>

namespace oryx
{

// Always compiled in (unlike ScopeTimer): it times whole runs and frames, not per-call-site hot loops.
class Timer
{
public:
    void start()
    {
        m_start = std::chrono::steady_clock::now();
        m_last = m_start;
    }
    void stop() { m_end = std::chrono::steady_clock::now(); }

    // Seconds since the previous tick (or start), restarting the interval; the first tick of a never-started Timer returns 0.
    double tick()
    {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        double delta = m_last == std::chrono::steady_clock::time_point{} ? 0.0 : std::chrono::duration<double>(now - m_last).count();
        m_last = now;
        return delta;
    }

    double tick_ms() { return tick() * 1000.0; }

    [[nodiscard]] double elapsed_seconds() const
    {
        return std::chrono::duration<double>(m_end - m_start).count();
    }

private:
    std::chrono::steady_clock::time_point m_start{};
    std::chrono::steady_clock::time_point m_end{};
    std::chrono::steady_clock::time_point m_last{};
};

} // namespace oryx
