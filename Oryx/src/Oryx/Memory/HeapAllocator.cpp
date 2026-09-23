#include "oxpch.h"
#include "Oryx/Memory/HeapAllocator.h"

namespace oryx
{

void* HeapAllocator::allocate(size_t size, size_t alignment)
{
    if (alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
    {
        return ::operator new(size);
    }
    return ::operator new(size, std::align_val_t(alignment));
}

void HeapAllocator::deallocate(void* pointer, size_t size, size_t alignment) noexcept
{
    if (alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
    {
        ::operator delete(pointer, size);
        return;
    }
    ::operator delete(pointer, size, std::align_val_t(alignment));
}

HeapAllocator& heap_allocator()
{
    // Never destroyed: the never-destroyed default pool keeps using it during static destruction.
    alignas(HeapAllocator) static std::byte storage[sizeof(HeapAllocator)];
    static HeapAllocator* instance = new (storage) HeapAllocator();
    return *instance;
}

} // namespace oryx
