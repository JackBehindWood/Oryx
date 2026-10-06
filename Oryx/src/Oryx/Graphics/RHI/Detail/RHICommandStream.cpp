#include "oxpch.h"
#include "Oryx/Graphics/RHI/Detail/RHICommandStream.h"

namespace oryx
{

RHICommandStream::RHICommandStream(RHICommandStream&& other) noexcept
    : m_arena(std::move(other.m_arena))
    , m_block_size(other.m_block_size)
    , m_head(std::exchange(other.m_head, nullptr))
    , m_tail(std::exchange(other.m_tail, nullptr))
    , m_count(std::exchange(other.m_count, 0))
{
}

RHICommandStream& RHICommandStream::operator=(RHICommandStream&& other) noexcept
{
    if (this != &other)
    {
        m_arena = std::move(other.m_arena);
        m_block_size = other.m_block_size;
        m_head = std::exchange(other.m_head, nullptr);
        m_tail = std::exchange(other.m_tail, nullptr);
        m_count = std::exchange(other.m_count, 0);
    }
    return *this;
}

void* RHICommandStream::allocate(size_t size, size_t alignment)
{
    if (!m_arena)
    {
        m_arena = create_unique<ArenaAllocator>(detail::object_allocator(), m_block_size);
    }
    return m_arena->allocate(size, alignment);
}

void RHICommandStream::link(RHICommand& command)
{
    if (m_tail != nullptr)
    {
        m_tail->m_next = &command;
    }
    else
    {
        m_head = &command;
    }
    m_tail = &command;
    ++m_count;
}

void RHICommandStream::clear()
{
    if (m_arena)
    {
        m_arena->reset();
    }
    m_head = nullptr;
    m_tail = nullptr;
    m_count = 0;
}

} // namespace oryx
