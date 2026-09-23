#pragma once

namespace oryx
{

// Every free is sized: deallocate gets the size and alignment that allocate was called with.
class IAllocator
{
public:
    virtual ~IAllocator() = default;

    [[nodiscard]] virtual void* allocate(size_t size, size_t alignment) = 0;
    virtual void deallocate(void* pointer, size_t size, size_t alignment) noexcept = 0;
};

} // namespace oryx
