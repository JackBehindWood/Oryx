#include "oxpch.h"
#include "Oryx/Debug/MemoryTracker.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(__APPLE__)
    #include <malloc/malloc.h>
#elif defined(_WIN32)
    #include <malloc.h>
#else
    #include <malloc.h>
#endif

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

// Must be __STDCPP_DEFAULT_NEW_ALIGNMENT__, not alignof(std::max_align_t): the plain (non-
// align_val_t) new/delete overloads below are what the compiler holds to that alignment, and on
// Apple Clang/AArch64 the two differ (16 vs 8).
constexpr size_t kDefaultAlignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

// The allocator's own usable-size bookkeeping, not a hand-rolled header: some libc++ containers
// (e.g. std::string's growth path) are pre-instantiated out-of-line in the system's libc++ dylib,
// so their internal `new` binds to the platform allocator directly rather than the overloads
// below - a header we wrote at alloc time would not exist for such a pointer, and freeing through
// it corrupted the heap (this is what actually made Release/Dist crash). Sizing and freeing every
// pointer through the platform itself works no matter which path allocated it.
size_t usable_size(void* pointer)
{
#if defined(__APPLE__)
    return malloc_size(pointer);
#elif defined(_WIN32)
    return _msize(pointer);
#else
    return malloc_usable_size(pointer);
#endif
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
    void* pointer = alignment <= kDefaultAlignment
        ? std::malloc(size)
        : std::aligned_alloc(alignment, (size + alignment - 1) / alignment * alignment);
    if (pointer == nullptr)
    {
        return nullptr;
    }

    record_allocation(usable_size(pointer));
    return pointer;
}

void tracked_free(void* pointer) noexcept
{
    if (pointer == nullptr)
    {
        return;
    }

    record_deallocation(usable_size(pointer));
    std::free(pointer);
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

void operator delete(void* pointer) noexcept { oryx::tracked_free(pointer); }
void operator delete[](void* pointer) noexcept { oryx::tracked_free(pointer); }
void operator delete(void* pointer, size_t) noexcept { oryx::tracked_free(pointer); }
void operator delete[](void* pointer, size_t) noexcept { oryx::tracked_free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { oryx::tracked_free(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { oryx::tracked_free(pointer); }

void operator delete(void* pointer, std::align_val_t) noexcept { oryx::tracked_free(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { oryx::tracked_free(pointer); }
void operator delete(void* pointer, size_t, std::align_val_t) noexcept { oryx::tracked_free(pointer); }
void operator delete[](void* pointer, size_t, std::align_val_t) noexcept { oryx::tracked_free(pointer); }
void operator delete(void* pointer, std::align_val_t, const std::nothrow_t&) noexcept { oryx::tracked_free(pointer); }
void operator delete[](void* pointer, std::align_val_t, const std::nothrow_t&) noexcept { oryx::tracked_free(pointer); }

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
