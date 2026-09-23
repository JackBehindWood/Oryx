#include "oxpch.h"
#include "Oryx/Memory/DefaultAllocator.h"

#include "Oryx/Memory/HeapAllocator.h"
#include "Oryx/Memory/PoolAllocator.h"

namespace oryx
{

namespace
{

struct DefaultStack
{
    PoolAllocator pool{ heap_allocator() };
    CountingAllocator counting{ pool };
};

} // namespace

CountingAllocator& default_allocator()
{
    // Never destroyed: objects freed during static destruction still return their blocks here.
    alignas(DefaultStack) static std::byte storage[sizeof(DefaultStack)];
    static DefaultStack* stack = new (storage) DefaultStack();
    return stack->counting;
}

} // namespace oryx
