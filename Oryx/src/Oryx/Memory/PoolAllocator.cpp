#include "oxpch.h"
#include "Oryx/Memory/PoolAllocator.h"

#include "Oryx/Memory/AsanPoison.h"

namespace oryx
{

namespace
{

// Keeps the first block of a chunk at the chunk's own 16-byte alignment.
constexpr size_t k_chunk_header_size = 16;

class SpinGuard
{
public:
    explicit SpinGuard(std::atomic_flag& flag)
        : m_flag(flag)
    {
        while (m_flag.test_and_set(std::memory_order_acquire))
        {
            while (m_flag.test(std::memory_order_relaxed))
            {
            }
        }
    }

    ~SpinGuard() { m_flag.clear(std::memory_order_release); }

    SpinGuard(const SpinGuard&) = delete;
    SpinGuard& operator=(const SpinGuard&) = delete;

private:
    std::atomic_flag& m_flag;
};

} // namespace

PoolAllocator::~PoolAllocator()
{
    for (SizeClass& size_class : m_classes)
    {
        Chunk* chunk = size_class.chunks;
        while (chunk != nullptr)
        {
            Chunk* next = chunk->next;
            OX_ASAN_UNPOISON(chunk, k_chunk_size);
            m_upstream.deallocate(chunk, k_chunk_size, k_max_alignment);
            chunk = next;
        }
    }
}

namespace
{

constexpr size_t k_granule = 8;
constexpr size_t k_granules = PoolAllocator::k_class_sizes.back() / k_granule + 1;

// class_index for every size in 8-byte steps, for 8- and 16-byte alignment; the linear search runs only at compile time.
constexpr std::array<uint8_t, k_granules> class_table(size_t alignment)
{
    std::array<uint8_t, k_granules> table{};
    for (size_t granule = 0; granule < k_granules; ++granule)
    {
        size_t size = granule * k_granule;
        size_t index = 0;
        while (index < PoolAllocator::k_class_sizes.size() && (PoolAllocator::k_class_sizes[index] < size || PoolAllocator::k_class_sizes[index] % alignment != 0))
        {
            ++index;
        }
        table[granule] = static_cast<uint8_t>(index);
    }
    return table;
}

constexpr std::array<uint8_t, k_granules> k_class_by8 = class_table(8);
constexpr std::array<uint8_t, k_granules> k_class_by16 = class_table(16);

} // namespace

size_t PoolAllocator::class_index(size_t size, size_t alignment)
{
    if (alignment > k_max_alignment || size > k_class_sizes.back())
    {
        return k_class_sizes.size();
    }
    size_t granule = (size + k_granule - 1) / k_granule;
    return alignment == k_max_alignment ? k_class_by16[granule] : k_class_by8[granule];
}

void* PoolAllocator::allocate(size_t size, size_t alignment)
{
    size_t index = class_index(std::max<size_t>(size, 1), alignment);
    if (index == k_class_sizes.size())
    {
        return m_upstream.allocate(size, alignment);
    }

    size_t block_size = k_class_sizes[index];
    SizeClass& size_class = m_classes[index];
    SpinGuard guard(size_class.lock);

    if (FreeBlock* block = size_class.free_list)
    {
        OX_ASAN_UNPOISON(block, block_size);
        size_class.free_list = block->next;
        return block;
    }

    if (size_class.cursor == nullptr || size_class.cursor + block_size > size_class.end)
    {
        refill(size_class, block_size);
    }
    std::byte* block = size_class.cursor;
    size_class.cursor += block_size;
    OX_ASAN_UNPOISON(block, block_size);
    return block;
}

void PoolAllocator::deallocate(void* pointer, size_t size, size_t alignment) noexcept
{
    size_t index = class_index(std::max<size_t>(size, 1), alignment);
    if (index == k_class_sizes.size())
    {
        m_upstream.deallocate(pointer, size, alignment);
        return;
    }

    SizeClass& size_class = m_classes[index];
    SpinGuard guard(size_class.lock);
    FreeBlock* block = static_cast<FreeBlock*>(pointer);
    block->next = size_class.free_list;
    size_class.free_list = block;
    OX_ASAN_POISON(block, k_class_sizes[index]);
}

void PoolAllocator::refill(SizeClass& size_class, size_t block_size)
{
    std::byte* memory = static_cast<std::byte*>(m_upstream.allocate(k_chunk_size, k_max_alignment));
    Chunk* chunk = reinterpret_cast<Chunk*>(memory);
    chunk->next = size_class.chunks;
    size_class.chunks = chunk;

    size_class.cursor = memory + k_chunk_header_size;
    size_class.end = memory + k_chunk_header_size + (k_chunk_size - k_chunk_header_size) / block_size * block_size;
    OX_ASAN_POISON(size_class.cursor, static_cast<size_t>(size_class.end - size_class.cursor));
}

} // namespace oryx
