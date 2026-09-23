#include "oxpch.h"
#include "Oryx/Memory/SmartPointers.h"

#include "Oryx/Memory/DefaultAllocator.h"

namespace oryx::detail
{

IAllocator& object_allocator()
{
    return default_allocator();
}

} // namespace oryx::detail
