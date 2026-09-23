#include "oxpch.h"
#include "Oryx/Debug/MemoryTracker.h"

#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(__APPLE__)
    #include <malloc/malloc.h>
#else
    #include <malloc.h>
#endif

namespace
{

// Must be __STDCPP_DEFAULT_NEW_ALIGNMENT__, not alignof(std::max_align_t): on Apple Clang/AArch64 the two differ (16 vs 8).
constexpr size_t kDefaultAlignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

// Sized by the platform, not a header of ours: libc++'s dylib allocates some blocks with the platform allocator that reach our delete.
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

void* platform_allocate(size_t size, size_t alignment) noexcept
{
    if (alignment <= kDefaultAlignment)
    {
        return std::malloc(size);
    }
#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, (size + alignment - 1) / alignment * alignment);
#endif
}

size_t platform_size(void* pointer, size_t alignment)
{
#if defined(_WIN32)
    if (alignment > kDefaultAlignment)
    {
        return _aligned_msize(pointer, alignment, 0);
    }
#endif
    (void)alignment;
    return usable_size(pointer);
}

void platform_free(void* pointer, size_t alignment) noexcept
{
#if defined(_WIN32)
    if (alignment > kDefaultAlignment)
    {
        _aligned_free(pointer);
        return;
    }
#endif
    (void)alignment;
    std::free(pointer);
}

void* tracked_allocate(size_t size, size_t alignment) noexcept
{
    void* pointer = platform_allocate(size, alignment);
    if (pointer == nullptr)
    {
        return nullptr;
    }

    oryx::detail::record_allocation(platform_size(pointer, alignment));
    return pointer;
}

void tracked_free(void* pointer, size_t alignment = kDefaultAlignment) noexcept
{
    if (pointer == nullptr)
    {
        return;
    }

    oryx::detail::record_deallocation(platform_size(pointer, alignment));
    platform_free(pointer, alignment);
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

size_t to_size(std::align_val_t alignment)
{
    return static_cast<size_t>(alignment);
}

} // namespace

void* operator new(size_t size) { return allocate_or_throw(size, kDefaultAlignment); }
void* operator new[](size_t size) { return allocate_or_throw(size, kDefaultAlignment); }
void* operator new(size_t size, const std::nothrow_t&) noexcept { return tracked_allocate(size, kDefaultAlignment); }
void* operator new[](size_t size, const std::nothrow_t&) noexcept { return tracked_allocate(size, kDefaultAlignment); }

void* operator new(size_t size, std::align_val_t alignment) { return allocate_or_throw(size, to_size(alignment)); }
void* operator new[](size_t size, std::align_val_t alignment) { return allocate_or_throw(size, to_size(alignment)); }
void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return tracked_allocate(size, to_size(alignment)); }
void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return tracked_allocate(size, to_size(alignment)); }

void operator delete(void* pointer) noexcept { tracked_free(pointer); }
void operator delete[](void* pointer) noexcept { tracked_free(pointer); }
void operator delete(void* pointer, size_t) noexcept { tracked_free(pointer); }
void operator delete[](void* pointer, size_t) noexcept { tracked_free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { tracked_free(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { tracked_free(pointer); }

void operator delete(void* pointer, std::align_val_t alignment) noexcept { tracked_free(pointer, to_size(alignment)); }
void operator delete[](void* pointer, std::align_val_t alignment) noexcept { tracked_free(pointer, to_size(alignment)); }
void operator delete(void* pointer, size_t, std::align_val_t alignment) noexcept { tracked_free(pointer, to_size(alignment)); }
void operator delete[](void* pointer, size_t, std::align_val_t alignment) noexcept { tracked_free(pointer, to_size(alignment)); }
void operator delete(void* pointer, std::align_val_t alignment, const std::nothrow_t&) noexcept { tracked_free(pointer, to_size(alignment)); }
void operator delete[](void* pointer, std::align_val_t alignment, const std::nothrow_t&) noexcept { tracked_free(pointer, to_size(alignment)); }
