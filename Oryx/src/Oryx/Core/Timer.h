#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

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

// Holds a loop to a fixed rate; with Timer, the only place the clock and sleeping are named.
class FramePacer
{
public:
    // 0 means uncapped.
    void set_rate(int32_t frames_per_second)
    {
        m_period = frames_per_second > 0 ? std::chrono::nanoseconds(1'000'000'000 / frames_per_second) : std::chrono::nanoseconds(0);
        reset();
    }

    [[nodiscard]] bool capped() const { return m_period.count() > 0; }

    // Makes the next frame due now.
    void reset() { m_next = std::chrono::steady_clock::now(); }

    // Blocks until the next frame is due, or returns at once when the loop is already late.
    void wait()
    {
        using clock = std::chrono::steady_clock;
        const clock::time_point now = clock::now();
        m_next += m_period;
        if (now >= m_next)
        {
            m_next = now;
            return;
        }
        // OS sleeps overshoot by about a millisecond, so sleep short of the deadline and yield-spin the rest.
        std::this_thread::sleep_until(m_next - std::chrono::milliseconds(1));
        while (clock::now() < m_next)
        {
            std::this_thread::yield();
        }
    }

    static void sleep_ms(int32_t milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

private:
    std::chrono::nanoseconds m_period{ 0 };
    std::chrono::steady_clock::time_point m_next{};
};

} // namespace oryx
