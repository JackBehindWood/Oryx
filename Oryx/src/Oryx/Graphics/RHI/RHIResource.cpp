#include "oxpch.h"
#include "Oryx/Graphics/RHI/RHIResource.h"

namespace oryx
{

constexpr uint64_t RHI_IDLE = std::numeric_limits<uint64_t>::max();

struct RHIRetireState
{
    struct Entry
    {
        RefCounted* object = nullptr;
        uint64_t serial = 0;
    };

    std::mutex mutex;
    // Pushed in non-decreasing serial order; [head, end) is pending.
    std::vector<Entry> entries;
    size_t head = 0;
    uint64_t serial = 0;
    std::vector<RHIDeviceLease*> leases;
    std::atomic<size_t> live{ 0 };
    std::unordered_set<const RHIResource*> live_set;

    RHIRetireState() { entries.reserve(64); }

    void compact_locked()
    {
        if (head == entries.size())
        {
            entries.clear();
            head = 0;
        }
        else if (head > entries.size() / 2)
        {
            entries.erase(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(head));
            head = 0;
        }
    }

    uint64_t threshold_locked() const
    {
        uint64_t threshold = RHI_IDLE;
        for (const RHIDeviceLease* lease : leases)
        {
            threshold = std::min(threshold, lease->m_completed);
        }
        return threshold;
    }

    void destroy(std::vector<Entry>& batch)
    {
        for (const Entry& entry : batch)
        {
            detail::destroy_now(*entry.object);
        }
        batch.clear();
    }

    // Destroying an object can release children, which re-enter retire(); loop until none are eligible.
    void collect()
    {
        thread_local std::vector<Entry> batch;
        for (;;)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                const uint64_t threshold = threshold_locked();
                size_t end = head;
                while (end < entries.size() && entries[end].serial < threshold)
                {
                    ++end;
                }
                if (end == head)
                {
                    return;
                }
                batch.assign(entries.begin() + static_cast<std::ptrdiff_t>(head), entries.begin() + static_cast<std::ptrdiff_t>(end));
                head = end;
                compact_locked();
            }
            destroy(batch);
        }
    }

    void drain()
    {
        thread_local std::vector<Entry> batch;
        for (;;)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (head == entries.size())
                {
                    return;
                }
                batch.assign(entries.begin() + static_cast<std::ptrdiff_t>(head), entries.end());
                entries.clear();
                head = 0;
            }
            destroy(batch);
        }
    }
};

namespace
{

// Immortal (constructed in static storage, never destroyed): a static that still holds an RHI*Ptr at process exit must release into a live queue.
RHIRetireState& state()
{
    alignas(RHIRetireState) static std::byte storage[sizeof(RHIRetireState)];
    static RHIRetireState& instance = *new (storage) RHIRetireState();
    return instance;
}

} // namespace

RHIResource::RHIResource()
{
    RHIRetireState& queue = state();
    std::lock_guard<std::mutex> lock(queue.mutex);
    queue.live_set.insert(this);
    queue.live.fetch_add(1, std::memory_order_relaxed);
}

RHIResource::~RHIResource()
{
    RHIRetireState& queue = state();
    std::lock_guard<std::mutex> lock(queue.mutex);
    queue.live_set.erase(this);
    queue.live.fetch_sub(1, std::memory_order_relaxed);
}

void RHIResource::on_last_release() noexcept
{
    RHIRetireState& queue = state();
    {
        std::lock_guard<std::mutex> lock(queue.mutex);
        if (!queue.leases.empty())
        {
            queue.entries.push_back({ this, queue.serial });
            return;
        }
    }
    detail::destroy_now(*this);
}

size_t RHIResource::live_count()
{
    return state().live.load(std::memory_order_relaxed);
}

std::string RHIResource::live_report()
{
    RHIRetireState& queue = state();
    std::map<std::string, size_t> counts;
    {
        std::lock_guard<std::mutex> lock(queue.mutex);
        for (const RHIResource* resource : queue.live_set)
        {
            ++counts[typeid(*resource).name()];
        }
    }
    std::string report;
    for (const std::pair<const std::string, size_t>& entry : counts)
    {
        report += (report.empty() ? "" : ", ") + std::to_string(entry.second) + " x " + entry.first;
    }
    return report;
}

size_t RHIResource::retired_pending()
{
    RHIRetireState& queue = state();
    std::lock_guard<std::mutex> lock(queue.mutex);
    return queue.entries.size() - queue.head;
}

uint64_t RHIResource::frame_serial()
{
    RHIRetireState& queue = state();
    std::lock_guard<std::mutex> lock(queue.mutex);
    return queue.serial;
}

RHIDeviceLease::RHIDeviceLease()
    : m_completed(RHI_IDLE)
{
    RHIRetireState& queue = state();
    std::lock_guard<std::mutex> lock(queue.mutex);
    queue.leases.push_back(this);
}

RHIDeviceLease::~RHIDeviceLease()
{
    RHIRetireState& queue = state();
    bool last = false;
    {
        std::lock_guard<std::mutex> lock(queue.mutex);
        queue.leases.erase(std::find(queue.leases.begin(), queue.leases.end(), this));
        last = queue.leases.empty();
    }
    if (last)
    {
        queue.drain();
        if (queue.live.load() != 0)
        {
            OX_CORE_ERROR("RHI device destroyed with {} live resources: {}", queue.live.load(), RHIResource::live_report());
        }
        OX_ASSERT(queue.live.load() == 0, "RHI device destroyed while RHI resources are still referenced");
    }
    else
    {
        queue.collect();
    }
}

uint64_t RHIDeviceLease::end_frame()
{
    RHIRetireState& queue = state();
    std::lock_guard<std::mutex> lock(queue.mutex);
    return queue.serial++;
}

void RHIDeviceLease::complete(uint64_t serial)
{
    RHIRetireState& queue = state();
    {
        std::lock_guard<std::mutex> lock(queue.mutex);
        m_completed = serial + 1;
    }
    queue.collect();
}

void RHIDeviceLease::begin_work()
{
    RHIRetireState& queue = state();
    std::lock_guard<std::mutex> lock(queue.mutex);
    if (m_completed == RHI_IDLE)
    {
        m_completed = 0;
    }
}

void RHIDeviceLease::set_idle()
{
    RHIRetireState& queue = state();
    {
        std::lock_guard<std::mutex> lock(queue.mutex);
        m_completed = RHI_IDLE;
    }
    queue.collect();
}

} // namespace oryx
