#pragma once

#include "Oryx/Memory/CountingAllocator.h"

namespace oryx
{

// Oryx's default memory source: counting over a size-class pool over the heap. Constructed on first use, never destroyed.
[[nodiscard]] CountingAllocator& default_allocator();

[[nodiscard]] inline MemoryStats default_allocator_stats()
{
    return default_allocator().counters().snapshot();
}

} // namespace oryx
