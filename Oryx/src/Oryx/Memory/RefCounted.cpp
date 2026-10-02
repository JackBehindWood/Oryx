#include "oxpch.h"
#include "Oryx/Memory/RefCounted.h"

namespace oryx::detail
{

void destroy_now(RefCounted& object) noexcept
{
    IAllocator* allocator = object.m_allocator;
    size_t size = object.m_size;
    size_t alignment = object.m_alignment;
    OX_ASSERT(allocator != nullptr, "RefCounted objects must be created by make_ref or allocate_ref");
    object.~RefCounted();
    allocator->deallocate(&object, size, alignment);
}

} // namespace oryx::detail
