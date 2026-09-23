#pragma once

#include "Oryx/Memory/IAllocator.h"

namespace oryx
{

// The global operator new/delete, so an executable's allocation census still sees this traffic.
class HeapAllocator final : public IAllocator
{
public:
    [[nodiscard]] void* allocate(size_t size, size_t alignment) override;
    void deallocate(void* pointer, size_t size, size_t alignment) noexcept override;
};

[[nodiscard]] HeapAllocator& heap_allocator();

} // namespace oryx
