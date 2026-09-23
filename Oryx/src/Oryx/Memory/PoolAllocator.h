#pragma once

#include "Oryx/Memory/IAllocator.h"

namespace oryx
{

// Size classes of 16..512 bytes carved from 64 KiB chunks; larger or over-aligned requests go straight upstream.
class PoolAllocator final : public IAllocator
{
public:
    static constexpr std::array<size_t, 11> kClassSizes = { 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512 };
    static constexpr size_t kMaxAlignment = 16;
    static constexpr size_t kChunkSize = 64 * 1024;

    explicit PoolAllocator(IAllocator& upstream)
        : m_upstream(upstream)
    {
    }

    ~PoolAllocator() override;

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    [[nodiscard]] void* allocate(size_t size, size_t alignment) override;
    void deallocate(void* pointer, size_t size, size_t alignment) noexcept override;

    // kClassSizes.size() when the request bypasses the pool.
    [[nodiscard]] static size_t class_index(size_t size, size_t alignment);

private:
    struct FreeBlock
    {
        FreeBlock* next;
    };

    struct Chunk
    {
        Chunk* next;
    };

    struct SizeClass
    {
        std::atomic_flag lock;
        FreeBlock* free_list = nullptr;
        std::byte* cursor = nullptr;
        std::byte* end = nullptr;
        Chunk* chunks = nullptr;
    };

    void refill(SizeClass& size_class, size_t block_size);

    IAllocator& m_upstream;
    std::array<SizeClass, kClassSizes.size()> m_classes{};
};

} // namespace oryx
