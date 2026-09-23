#pragma once

#include "Oryx/Memory/IAllocator.h"

namespace oryx
{

// Bump allocation with mark/rewind; deallocate is a no-op and rewinding runs no destructors. Not thread-safe.
class ArenaAllocator final : public IAllocator
{
public:
    struct Mark
    {
        void* block = nullptr;
        size_t used = 0;
    };

    explicit ArenaAllocator(IAllocator& upstream, size_t block_size = 64 * 1024)
        : m_upstream(upstream)
        , m_block_size(block_size)
    {
    }

    ~ArenaAllocator() override;

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    [[nodiscard]] void* allocate(size_t size, size_t alignment) override;
    void deallocate(void*, size_t, size_t) noexcept override {}

    [[nodiscard]] Mark mark() const;
    void rewind(const Mark& mark);
    void reset() { rewind(Mark{}); }

private:
    struct Block
    {
        Block* next;
        size_t capacity;
    };

    Block* add_block_after_current(size_t capacity);

    IAllocator& m_upstream;
    size_t m_block_size;
    Block* m_first = nullptr;
    Block* m_current = nullptr;
    size_t m_used = 0;
};

} // namespace oryx
