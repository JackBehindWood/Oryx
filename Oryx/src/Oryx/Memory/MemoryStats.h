#pragma once

namespace oryx
{

struct MemoryStats
{
    uint64_t allocation_count = 0;
    uint64_t deallocation_count = 0;
    uint64_t bytes_allocated = 0;
    uint64_t bytes_freed = 0;
    int64_t live_bytes = 0;
    int64_t peak_live_bytes = 0;
};

// peak_live_bytes is the peak growth above before.live_bytes, so take `before` right after a reset_peak().
MemoryStats memory_delta(const MemoryStats& before, const MemoryStats& after);

// Relaxed atomics: constant-initialisable, so a counter can be used during static initialisation.
class MemoryCounters
{
public:
    constexpr MemoryCounters() = default;

    MemoryCounters(const MemoryCounters&) = delete;
    MemoryCounters& operator=(const MemoryCounters&) = delete;

    void record_allocation(size_t size)
    {
        m_allocation_count.fetch_add(1, std::memory_order_relaxed);
        m_bytes_allocated.fetch_add(size, std::memory_order_relaxed);

        int64_t live = m_live_bytes.fetch_add(static_cast<int64_t>(size), std::memory_order_relaxed) + static_cast<int64_t>(size);
        int64_t peak = m_peak_live_bytes.load(std::memory_order_relaxed);
        while (live > peak && !m_peak_live_bytes.compare_exchange_weak(peak, live, std::memory_order_relaxed))
        {
        }
    }

    void record_deallocation(size_t size)
    {
        m_deallocation_count.fetch_add(1, std::memory_order_relaxed);
        m_bytes_freed.fetch_add(size, std::memory_order_relaxed);
        m_live_bytes.fetch_sub(static_cast<int64_t>(size), std::memory_order_relaxed);
    }

    [[nodiscard]] MemoryStats snapshot() const;
    void reset_peak();

    // reset_peak() then snapshot(), the `before` that memory_delta() expects.
    MemoryStats begin_measurement();

private:
    std::atomic<uint64_t> m_allocation_count{ 0 };
    std::atomic<uint64_t> m_deallocation_count{ 0 };
    std::atomic<uint64_t> m_bytes_allocated{ 0 };
    std::atomic<uint64_t> m_bytes_freed{ 0 };
    std::atomic<int64_t> m_live_bytes{ 0 };
    std::atomic<int64_t> m_peak_live_bytes{ 0 };
};

} // namespace oryx
