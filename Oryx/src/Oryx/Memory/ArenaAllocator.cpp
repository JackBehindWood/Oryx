#include "oxpch.h"
#include "Oryx/Memory/ArenaAllocator.h"

#include "Oryx/Memory/AsanPoison.h"

namespace oryx
{

namespace
{

constexpr size_t kBlockHeaderSize = 16;
constexpr size_t kBlockAlignment = 16;

std::byte* data_of(void* block)
{
    return static_cast<std::byte*>(block) + kBlockHeaderSize;
}

} // namespace

ArenaAllocator::~ArenaAllocator()
{
    Block* block = m_first;
    while (block != nullptr)
    {
        Block* next = block->next;
        size_t capacity = block->capacity;
        OX_ASAN_UNPOISON(data_of(block), capacity);
        m_upstream.deallocate(block, kBlockHeaderSize + capacity, kBlockAlignment);
        block = next;
    }
}

void* ArenaAllocator::allocate(size_t size, size_t alignment)
{
    size = std::max<size_t>(size, 1);
    while (true)
    {
        if (m_current != nullptr)
        {
            uintptr_t base = reinterpret_cast<uintptr_t>(data_of(m_current));
            uintptr_t aligned = (base + m_used + alignment - 1) / alignment * alignment;
            size_t offset = static_cast<size_t>(aligned - base);
            if (offset + size <= m_current->capacity)
            {
                m_used = offset + size;
                void* result = reinterpret_cast<void*>(aligned);
                OX_ASAN_UNPOISON(result, size);
                return result;
            }
        }

        Block* next = m_current != nullptr ? m_current->next : m_first;
        m_current = next != nullptr && next->capacity >= size + alignment ? next : add_block_after_current(std::max(m_block_size, size + alignment));
        m_used = 0;
    }
}

ArenaAllocator::Mark ArenaAllocator::mark() const
{
    return Mark{ m_current, m_used };
}

void ArenaAllocator::rewind(const Mark& mark)
{
    Block* stop = m_current != nullptr ? m_current->next : nullptr;
    Block* block = mark.block != nullptr ? static_cast<Block*>(mark.block) : m_first;
    size_t from = mark.block != nullptr ? mark.used : 0;
    for (; block != nullptr && block != stop; block = block->next, from = 0)
    {
        OX_ASAN_POISON(data_of(block) + from, block->capacity - from);
    }

    m_current = static_cast<Block*>(mark.block);
    m_used = mark.used;
}

ArenaAllocator::Block* ArenaAllocator::add_block_after_current(size_t capacity)
{
    Block* block = static_cast<Block*>(m_upstream.allocate(kBlockHeaderSize + capacity, kBlockAlignment));
    block->capacity = capacity;
    if (m_current != nullptr)
    {
        block->next = m_current->next;
        m_current->next = block;
    }
    else
    {
        block->next = m_first;
        m_first = block;
    }
    OX_ASAN_POISON(data_of(block), capacity);
    return block;
}

} // namespace oryx
