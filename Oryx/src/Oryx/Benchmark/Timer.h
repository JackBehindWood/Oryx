#pragma once

#include <chrono>

namespace oryx
{

// Always compiled in (unlike ScopeTimer): it times a whole benchmark run, not a per-call-site hot loop.
class Timer
{
public:
    void start() { m_start = std::chrono::steady_clock::now(); }
    void stop() { m_end = std::chrono::steady_clock::now(); }

    [[nodiscard]] double elapsed_seconds() const
    {
        return std::chrono::duration<double>(m_end - m_start).count();
    }

private:
    std::chrono::steady_clock::time_point m_start{};
    std::chrono::steady_clock::time_point m_end{};
};

} // namespace oryx
