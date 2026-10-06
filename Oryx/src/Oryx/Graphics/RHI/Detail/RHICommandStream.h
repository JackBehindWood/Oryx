#pragma once

#include "Oryx/Graphics/RHI/Detail/RHICommand.h"
#include "Oryx/Memory/ArenaAllocator.h"

namespace oryx
{

// Arena storage plus the intrusive chain of recorded commands; knows nothing about resources or validation.
class RHICommandStream
{
public:
    class Iterator
    {
    public:
        explicit Iterator(const RHICommand* command = nullptr)
            : m_command(command)
        {
        }

        const RHICommand& operator*() const { return *m_command; }
        const RHICommand* operator->() const { return m_command; }

        Iterator& operator++()
        {
            m_command = m_command->next();
            return *this;
        }

        friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_command == b.m_command; }

    private:
        const RHICommand* m_command;
    };

    static constexpr size_t DEFAULT_BLOCK_SIZE = 16 * 1024;

    explicit RHICommandStream(size_t block_size = DEFAULT_BLOCK_SIZE)
        : m_block_size(block_size)
    {
    }

    RHICommandStream(const RHICommandStream&) = delete;
    RHICommandStream& operator=(const RHICommandStream&) = delete;
    RHICommandStream(RHICommandStream&& other) noexcept;
    RHICommandStream& operator=(RHICommandStream&& other) noexcept;

    template<typename T, typename... Args>
    T& emplace(Args&&... args)
    {
        static_assert(std::is_base_of_v<RHICommand, T>);
        static_assert(std::is_trivially_destructible_v<T>, "commands are never destroyed individually");
        T* command = new (allocate(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
        link(*command);
        return *command;
    }

    [[nodiscard]] void* allocate(size_t size, size_t alignment);

    // Drops every command; the arena's memory is kept for reuse.
    void clear();

    [[nodiscard]] Iterator begin() const { return Iterator(m_head); }
    [[nodiscard]] Iterator end() const { return Iterator(); }
    [[nodiscard]] size_t size() const { return m_count; }

private:
    void link(RHICommand& command);

    // Held by pointer because the arena is not movable.
    UniquePtr<ArenaAllocator> m_arena;
    size_t m_block_size;
    RHICommand* m_head = nullptr;
    RHICommand* m_tail = nullptr;
    size_t m_count = 0;
};

} // namespace oryx
