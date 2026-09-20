#include "oxpch.h"
#include "Oryx/Debug/MemoryTracker.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace oryx
{

namespace
{

// Constant-initialised: Registry self-registration calls operator new during static init, before any dynamic initialiser runs.
std::atomic<uint64_t> g_allocation_count{ 0 };
std::atomic<uint64_t> g_deallocation_count{ 0 };
std::atomic<uint64_t> g_bytes_allocated{ 0 };
std::atomic<uint64_t> g_bytes_freed{ 0 };
std::atomic<int64_t> g_live_bytes{ 0 };
std::atomic<int64_t> g_peak_live_bytes{ 0 };

constexpr size_t kDefaultAlignment = alignof(std::max_align_t);

size_t header_size(size_t alignment)
{
    return std::max(alignment, kDefaultAlignment);
}

void record_allocation(size_t size)
{
    g_allocation_count.fetch_add(1, std::memory_order_relaxed);
    g_bytes_allocated.fetch_add(size, std::memory_order_relaxed);

    int64_t live = g_live_bytes.fetch_add(static_cast<int64_t>(size), std::memory_order_relaxed) + static_cast<int64_t>(size);
    int64_t peak = g_peak_live_bytes.load(std::memory_order_relaxed);
    while (live > peak && !g_peak_live_bytes.compare_exchange_weak(peak, live, std::memory_order_relaxed))
    {
    }
}

void record_deallocation(size_t size)
{
    g_deallocation_count.fetch_add(1, std::memory_order_relaxed);
    g_bytes_freed.fetch_add(size, std::memory_order_relaxed);
    g_live_bytes.fetch_sub(static_cast<int64_t>(size), std::memory_order_relaxed);
}

void* tracked_allocate(size_t size, size_t alignment) noexcept
{
    size_t header = header_size(alignment);
    if (size > SIZE_MAX - header - alignment)
    {
        return nullptr;
    }

    size_t total = size + header;
    void* raw = alignment <= kDefaultAlignment
        ? std::malloc(total)
        : std::aligned_alloc(alignment, (total + alignment - 1) / alignment * alignment);
    if (raw == nullptr)
    {
        return nullptr;
    }

    *static_cast<size_t*>(raw) = size;
    record_allocation(size);
    return static_cast<std::byte*>(raw) + header;
}

void tracked_free(void* pointer, size_t alignment) noexcept
{
    if (pointer == nullptr)
    {
        return;
    }

    void* raw = static_cast<std::byte*>(pointer) - header_size(alignment);
    record_deallocation(*static_cast<size_t*>(raw));
    std::free(raw);
}

void* allocate_or_throw(size_t size, size_t alignment)
{
    void* pointer = tracked_allocate(size, alignment);
    if (pointer == nullptr)
    {
        throw std::bad_alloc();
    }
    return pointer;
}

} // namespace

MemoryStats MemoryTracker::snapshot()
{
    MemoryStats stats;
    stats.allocation_count = g_allocation_count.load(std::memory_order_relaxed);
    stats.deallocation_count = g_deallocation_count.load(std::memory_order_relaxed);
    stats.bytes_allocated = g_bytes_allocated.load(std::memory_order_relaxed);
    stats.bytes_freed = g_bytes_freed.load(std::memory_order_relaxed);
    stats.live_bytes = g_live_bytes.load(std::memory_order_relaxed);
    stats.peak_live_bytes = g_peak_live_bytes.load(std::memory_order_relaxed);
    return stats;
}

void MemoryTracker::reset_peak()
{
    g_peak_live_bytes.store(g_live_bytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

MemoryStats MemoryTracker::begin_measurement()
{
    reset_peak();
    return snapshot();
}

} // namespace oryx

void* operator new(size_t size) { return oryx::allocate_or_throw(size, oryx::kDefaultAlignment); }
void* operator new[](size_t size) { return oryx::allocate_or_throw(size, oryx::kDefaultAlignment); }
void* operator new(size_t size, const std::nothrow_t&) noexcept { return oryx::tracked_allocate(size, oryx::kDefaultAlignment); }
void* operator new[](size_t size, const std::nothrow_t&) noexcept { return oryx::tracked_allocate(size, oryx::kDefaultAlignment); }

void* operator new(size_t size, std::align_val_t alignment) { return oryx::allocate_or_throw(size, static_cast<size_t>(alignment)); }
void* operator new[](size_t size, std::align_val_t alignment) { return oryx::allocate_or_throw(size, static_cast<size_t>(alignment)); }
void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return oryx::tracked_allocate(size, static_cast<size_t>(alignment)); }
void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return oryx::tracked_allocate(size, static_cast<size_t>(alignment)); }

void operator delete(void* pointer) noexcept { oryx::tracked_free(pointer, oryx::kDefaultAlignment); }
void operator delete[](void* pointer) noexcept { oryx::tracked_free(pointer, oryx::kDefaultAlignment); }
void operator delete(void* pointer, size_t) noexcept { oryx::tracked_free(pointer, oryx::kDefaultAlignment); }
void operator delete[](void* pointer, size_t) noexcept { oryx::tracked_free(pointer, oryx::kDefaultAlignment); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { oryx::tracked_free(pointer, oryx::kDefaultAlignment); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { oryx::tracked_free(pointer, oryx::kDefaultAlignment); }

void operator delete(void* pointer, std::align_val_t alignment) noexcept { oryx::tracked_free(pointer, static_cast<size_t>(alignment)); }
void operator delete[](void* pointer, std::align_val_t alignment) noexcept { oryx::tracked_free(pointer, static_cast<size_t>(alignment)); }
void operator delete(void* pointer, size_t, std::align_val_t alignment) noexcept { oryx::tracked_free(pointer, static_cast<size_t>(alignment)); }
void operator delete[](void* pointer, size_t, std::align_val_t alignment) noexcept { oryx::tracked_free(pointer, static_cast<size_t>(alignment)); }
void operator delete(void* pointer, std::align_val_t alignment, const std::nothrow_t&) noexcept { oryx::tracked_free(pointer, static_cast<size_t>(alignment)); }
void operator delete[](void* pointer, std::align_val_t alignment, const std::nothrow_t&) noexcept { oryx::tracked_free(pointer, static_cast<size_t>(alignment)); }

namespace oryx
{

MemoryStats memory_delta(const MemoryStats& before, const MemoryStats& after)
{
    MemoryStats delta;
    delta.allocation_count = after.allocation_count - before.allocation_count;
    delta.deallocation_count = after.deallocation_count - before.deallocation_count;
    delta.bytes_allocated = after.bytes_allocated - before.bytes_allocated;
    delta.bytes_freed = after.bytes_freed - before.bytes_freed;
    delta.live_bytes = after.live_bytes - before.live_bytes;
    delta.peak_live_bytes = std::max<int64_t>(after.peak_live_bytes - before.live_bytes, 0);
    return delta;
}

} // namespace oryx
