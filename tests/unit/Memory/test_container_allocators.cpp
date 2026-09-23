#include "doctest.h"

#include "Oryx/Containers/FlatHashMap.h"
#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Memory/CountingAllocator.h"
#include "Oryx/Memory/HeapAllocator.h"

using namespace oryx;

TEST_SUITE("memory")
{

TEST_CASE("SmallVector stays inline without touching its allocator and spills into it with exact sizes")
{
    CountingAllocator counting(heap_allocator());
    {
        SmallVector<int64_t, 4> values(counting);
        for (int64_t i = 0; i < 4; ++i)
        {
            values.push_back(i);
        }
        CHECK(counting.counters().snapshot().allocation_count == 0);

        values.push_back(4);
        MemoryStats spilled = counting.counters().snapshot();
        CHECK(spilled.allocation_count == 1);
        CHECK(spilled.bytes_allocated == 8 * sizeof(int64_t));
        CHECK(values[4] == 4);
    }
    MemoryStats done = counting.counters().snapshot();
    CHECK(done.deallocation_count == 1);
    CHECK(done.live_bytes == 0);
}

TEST_CASE("a moved SmallVector frees its buffer to the allocator it came from, and a copy uses the source's allocator")
{
    CountingAllocator first(heap_allocator());
    CountingAllocator second(heap_allocator());
    {
        SmallVector<int32_t, 2> source(first);
        for (int32_t i = 0; i < 5; ++i)
        {
            source.push_back(i);
        }

        SmallVector<int32_t, 2> target(second);
        target = std::move(source);
        CHECK(target.size() == 5);

        SmallVector<int32_t, 2> copy(target);
        CHECK(copy == target);
        CHECK(second.counters().snapshot().allocation_count == 0);
    }
    CHECK(first.counters().snapshot().live_bytes == 0);
    CHECK(first.counters().snapshot().deallocation_count == first.counters().snapshot().allocation_count);
    CHECK(second.counters().snapshot().allocation_count == 0);
}

TEST_CASE("FlatHashMap grows into its allocator and returns every block when destroyed")
{
    CountingAllocator counting(heap_allocator());
    {
        FlatHashMap<int32_t, int32_t, 4> map(counting);
        map.insert_or_assign(1, 10);
        map.insert_or_assign(2, 20);
        CHECK(counting.counters().snapshot().allocation_count == 0);

        for (int32_t key = 3; key < 40; ++key)
        {
            map.insert_or_assign(key, key * 10);
        }
        CHECK(map.size() == 39);
        CHECK(*map.find(17) == 170);
        CHECK(map.erase(17));
        CHECK(map.find(17) == nullptr);
        CHECK(counting.counters().snapshot().allocation_count > 0);
    }
    MemoryStats done = counting.counters().snapshot();
    CHECK(done.allocation_count == done.deallocation_count);
    CHECK(done.live_bytes == 0);
}

} // TEST_SUITE("memory")
