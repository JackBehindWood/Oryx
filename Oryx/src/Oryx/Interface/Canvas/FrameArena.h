#pragma once

namespace oryx
{

inline constexpr size_t k_default_arena_chunk_size = 16 * 1024; // 16 KiB, enough for a few hundred layout nodes and a few dozen formatted strings.

// Bump allocator for one frame's scratch (formatted text, layout nodes). `reset` rewinds and keeps the chunks, so a warm arena allocates nothing; memory handed out is valid until the next reset.
// Only trivially destructible types fit, since nothing is destroyed.
class FrameArena
{
public:
    explicit FrameArena(size_t chunk_size = k_default_arena_chunk_size)
        : m_chunk_size(chunk_size)
    {
    }
    ~FrameArena();

    FrameArena(const FrameArena&) = delete;
    FrameArena& operator=(const FrameArena&) = delete;

    void reset();
    void clear();

    [[nodiscard]] void* allocate(size_t size, size_t alignment);

    template<typename T>
    [[nodiscard]] std::span<T> allocate_array(size_t count)
    {
        static_assert(std::is_trivially_destructible_v<T>, "the arena never runs destructors");
        return std::span<T>(static_cast<T*>(allocate(sizeof(T) * count, alignof(T))), count);
    }

    // Copies the text into the arena.
    [[nodiscard]] std::string_view store(std::string_view text);
    // printf-style; the result is NUL-terminated inside the arena and the view excludes the terminator.
    [[nodiscard]] std::string_view format(const char* format, ...) __attribute__((format(printf, 2, 3)));

    [[nodiscard]] size_t used() const { return m_used_total; }
    [[nodiscard]] size_t capacity() const;

private:
    // One block holding this header followed by `size` bytes that `data` points at; blocks form a singly linked list that reset walks again from the head.
    struct Chunk
    {
        Chunk* next = nullptr;
        uint8_t* data = nullptr;
        size_t size = 0;
        size_t used = 0;
    };

    Chunk* m_head = nullptr;
    Chunk* m_tail = nullptr;
    Chunk* m_current = nullptr;
    size_t m_used_total = 0;
    size_t m_chunk_size;
};

} // namespace oryx
