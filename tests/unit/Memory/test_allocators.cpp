#include "doctest.h"

#include "Oryx/Memory/ArenaAllocator.h"
#include "Oryx/Memory/AsanPoison.h"
#include "Oryx/Memory/CountingAllocator.h"
#include "Oryx/Memory/DefaultAllocator.h"
#include "Oryx/Memory/HeapAllocator.h"
#include "Oryx/Memory/PoolAllocator.h"

using namespace oryx;

namespace
{

bool is_aligned(const void* pointer, size_t alignment)
{
    return reinterpret_cast<uintptr_t>(pointer) % alignment == 0;
}

} // namespace

TEST_SUITE("memory")
{

TEST_CASE("HeapAllocator honours default and over-alignment")
{
    HeapAllocator& heap = heap_allocator();
    for (size_t alignment : { size_t{ 8 }, size_t{ 16 }, size_t{ 64 }, size_t{ 256 } })
    {
        void* pointer = heap.allocate(100, alignment);
        CHECK(is_aligned(pointer, alignment));
        std::memset(pointer, 0xAB, 100);
        heap.deallocate(pointer, 100, alignment);
    }
}

TEST_CASE("PoolAllocator picks the smallest class that fits the size and the alignment")
{
    CHECK(PoolAllocator::class_index(1, 1) == 0);
    CHECK(PoolAllocator::kClassSizes[PoolAllocator::class_index(17, 8)] == 24);
    CHECK(PoolAllocator::kClassSizes[PoolAllocator::class_index(17, 16)] == 32);
    CHECK(PoolAllocator::kClassSizes[PoolAllocator::class_index(40, 16)] == 48);
    CHECK(PoolAllocator::kClassSizes[PoolAllocator::class_index(512, 16)] == 512);
    CHECK(PoolAllocator::class_index(513, 8) == PoolAllocator::kClassSizes.size());
    CHECK(PoolAllocator::class_index(8, 32) == PoolAllocator::kClassSizes.size());
}

TEST_CASE("PoolAllocator hands out aligned, distinct blocks for every size and reuses freed ones")
{
    CountingAllocator upstream(heap_allocator());
    {
        PoolAllocator pool(upstream);
        for (size_t alignment : { size_t{ 1 }, size_t{ 8 }, size_t{ 16 } })
        {
            for (size_t size = 1; size <= 512; size += 7)
            {
                void* first = pool.allocate(size, alignment);
                void* second = pool.allocate(size, alignment);
                CHECK(is_aligned(first, alignment));
                CHECK(is_aligned(second, alignment));
                CHECK(first != second);
                std::memset(first, 0x11, size);
                std::memset(second, 0x22, size);
                CHECK(*static_cast<unsigned char*>(first) == 0x11);

                pool.deallocate(second, size, alignment);
                CHECK(pool.allocate(size, alignment) == second);
                pool.deallocate(second, size, alignment);
                pool.deallocate(first, size, alignment);
            }
        }
    }
    CHECK(upstream.counters().snapshot().live_bytes == 0);
}

TEST_CASE("PoolAllocator takes whole chunks from upstream and sends large or over-aligned requests straight there")
{
    CountingAllocator upstream(heap_allocator());
    PoolAllocator pool(upstream);

    void* small = pool.allocate(40, 8);
    MemoryStats after_first = upstream.counters().snapshot();
    CHECK(after_first.allocation_count == 1);
    CHECK(after_first.bytes_allocated == PoolAllocator::kChunkSize);

    void* second_small = pool.allocate(40, 8);
    CHECK(upstream.counters().snapshot().allocation_count == 1);

    void* large = pool.allocate(1000, 8);
    void* over_aligned = pool.allocate(64, 64);
    CHECK(is_aligned(over_aligned, 64));
    MemoryStats after_bypass = upstream.counters().snapshot();
    CHECK(after_bypass.allocation_count == 3);
    CHECK(after_bypass.bytes_allocated == PoolAllocator::kChunkSize + 1000 + 64);

    pool.deallocate(large, 1000, 8);
    pool.deallocate(over_aligned, 64, 64);
    pool.deallocate(second_small, 40, 8);
    pool.deallocate(small, 40, 8);
    CHECK(upstream.counters().snapshot().live_bytes == PoolAllocator::kChunkSize);
}

TEST_CASE("CountingAllocator over a pool balances counts and bytes and tracks the peak")
{
    PoolAllocator pool(heap_allocator());
    CountingAllocator counting(pool);
    MemoryStats before = counting.counters().begin_measurement();

    std::vector<std::pair<void*, size_t>> blocks;
    for (size_t i = 0; i < 1000; ++i)
    {
        size_t size = 1 + (i * 37) % 700;
        blocks.emplace_back(counting.allocate(size, 8), size);
    }
    int64_t expected_peak = 0;
    for (const std::pair<void*, size_t>& block : blocks)
    {
        expected_peak += static_cast<int64_t>(block.second);
    }
    for (const std::pair<void*, size_t>& block : blocks)
    {
        counting.deallocate(block.first, block.second, 8);
    }

    MemoryStats delta = memory_delta(before, counting.counters().snapshot());
    CHECK(delta.allocation_count == 1000);
    CHECK(delta.deallocation_count == 1000);
    CHECK(delta.bytes_allocated == delta.bytes_freed);
    CHECK(delta.live_bytes == 0);
    CHECK(delta.peak_live_bytes == expected_peak);
}

TEST_CASE("PoolAllocator stays consistent under concurrent allocation from several threads")
{
    CountingAllocator upstream(heap_allocator());
    {
        PoolAllocator pool(upstream);
        CountingAllocator counting(pool);
        std::atomic<int32_t> corrupted{ 0 };

        std::vector<std::thread> threads;
        for (int32_t thread_id = 0; thread_id < 4; ++thread_id)
        {
            threads.emplace_back([&counting, &corrupted, thread_id]()
            {
                std::vector<std::pair<unsigned char*, size_t>> live;
                for (int32_t round = 0; round < 20000; ++round)
                {
                    size_t size = 1 + static_cast<size_t>((round * 13 + thread_id * 7) % 300);
                    unsigned char* block = static_cast<unsigned char*>(counting.allocate(size, 8));
                    std::memset(block, thread_id + 1, size);
                    live.emplace_back(block, size);
                    if (live.size() > 64)
                    {
                        for (const std::pair<unsigned char*, size_t>& entry : live)
                        {
                            if (entry.first[0] != thread_id + 1 || entry.first[entry.second - 1] != thread_id + 1)
                            {
                                corrupted.fetch_add(1);
                            }
                            counting.deallocate(entry.first, entry.second, 8);
                        }
                        live.clear();
                    }
                }
                for (const std::pair<unsigned char*, size_t>& entry : live)
                {
                    counting.deallocate(entry.first, entry.second, 8);
                }
            });
        }
        for (std::thread& thread : threads)
        {
            thread.join();
        }

        MemoryStats stats = counting.counters().snapshot();
        CHECK(corrupted.load() == 0);
        CHECK(stats.allocation_count == 80000);
        CHECK(stats.allocation_count == stats.deallocation_count);
        CHECK(stats.live_bytes == 0);
    }
    CHECK(upstream.counters().snapshot().live_bytes == 0);
}

TEST_CASE("ArenaAllocator aligns, rewinds to a mark, grows for large requests and returns every block")
{
    CountingAllocator upstream(heap_allocator());
    {
        ArenaAllocator arena(upstream, 256);

        void* a = arena.allocate(10, 1);
        void* b = arena.allocate(8, 64);
        CHECK(is_aligned(b, 64));
        CHECK(a != b);

        ArenaAllocator::Mark mark = arena.mark();
        void* c = arena.allocate(24, 8);
        void* big = arena.allocate(1000, 16);
        CHECK(is_aligned(big, 16));
        std::memset(big, 0x5A, 1000);

        arena.rewind(mark);
        CHECK(arena.allocate(24, 8) == c);

        arena.reset();
        CHECK(arena.allocate(10, 1) == a);
        CHECK(upstream.counters().snapshot().allocation_count == 2);
    }
    CHECK(upstream.counters().snapshot().live_bytes == 0);
}

TEST_CASE("default_allocator is one counting pool shared by every caller")
{
    CHECK(&default_allocator() == &default_allocator());

    MemoryStats before = default_allocator().counters().begin_measurement();
    void* pointer = default_allocator().allocate(48, 16);
    CHECK(is_aligned(pointer, 16));
    MemoryStats during = memory_delta(before, default_allocator_stats());
    default_allocator().deallocate(pointer, 48, 16);
    MemoryStats after = memory_delta(before, default_allocator_stats());

    CHECK(during.allocation_count == 1);
    CHECK(during.bytes_allocated == 48);
    CHECK(after.deallocation_count == 1);
    CHECK(after.live_bytes == 0);
}

#ifdef OX_ASAN_ENABLED
TEST_CASE("freed pool blocks and rewound arena memory are poisoned for AddressSanitizer")
{
    PoolAllocator pool(heap_allocator());
    void* block = pool.allocate(64, 8);
    CHECK(__asan_address_is_poisoned(block) == 0);
    pool.deallocate(block, 64, 8);
    CHECK(__asan_address_is_poisoned(block) != 0);
    CHECK(pool.allocate(64, 8) == block);
    CHECK(__asan_address_is_poisoned(block) == 0);
    pool.deallocate(block, 64, 8);

    ArenaAllocator arena(heap_allocator());
    ArenaAllocator::Mark mark = arena.mark();
    void* scratch = arena.allocate(32, 8);
    CHECK(__asan_address_is_poisoned(scratch) == 0);
    arena.rewind(mark);
    CHECK(__asan_address_is_poisoned(scratch) != 0);
}
#endif

} // TEST_SUITE("memory")
