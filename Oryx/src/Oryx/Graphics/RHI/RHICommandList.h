#pragma once

#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Graphics/RHI/RHICommand.h"
#include "Oryx/Memory/ArenaAllocator.h"

namespace oryx
{

// Records and validates; every recording method throws Error on an invalid call.
// Commands live in the list's own arena and point at their resources; the list retains each resource when it is recorded, so the
// caller may drop its own references before IRHI::submit, which consumes the list and takes over the retained references.
class RHICommandList final
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

    explicit RHICommandList(size_t initial_block_size = DEFAULT_BLOCK_SIZE)
        : m_block_size(initial_block_size)
    {
    }
    ~RHICommandList() = default;
    RHICommandList(const RHICommandList&) = delete;
    RHICommandList& operator=(const RHICommandList&) = delete;
    RHICommandList(RHICommandList&& other) noexcept;
    RHICommandList& operator=(RHICommandList&& other) noexcept;

    void begin_pass(RHIRenderTarget* target, const RHIClear& clear = {});
    void set_pipeline(RHIGraphicsPipeline* pipeline);
    void set_vertex_buffer(uint32_t slot, RHIBuffer* buffer, uint32_t offset = 0);
    void set_index_buffer(RHIBuffer* buffer, uint32_t offset = 0, bool index32 = true);
    void bind(RHIBindingId binding, RHIBuffer* buffer);
    void bind(RHIBindingId binding, RHITexture* texture);
    void bind(RHIBindingId binding, RHISampler* sampler);
    void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0);
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0);
    void end_pass();

    // Replays every command into the backend's context, in recording order.
    void execute(IRHICommandContext& context) const;

    // Drops all commands and releases the retained resources; the arena's memory is kept for reuse.
    void clear();

    // For backends: moves the retained resources into `sink` and clears the list.
    void drain_into(std::vector<Ref<RHIResource>>& sink);

    [[nodiscard]] Iterator begin() const { return Iterator(m_head); }
    [[nodiscard]] Iterator end() const { return Iterator(); }
    [[nodiscard]] size_t size() const { return m_count; }
    [[nodiscard]] bool empty() const { return m_count == 0; }
    [[nodiscard]] bool in_pass() const { return m_in_pass; }
    [[nodiscard]] size_t retained_count() const { return m_retained.size(); }

private:
    template<typename T, typename... Args>
    void emplace(Args&&... args);

    void retain(RHIResource& resource);
    void retain_in_slot(uint32_t slot, RHIResource& resource);
    void require_pass(const char* what) const;
    void require_pipeline(const char* what) const;

    static constexpr uint32_t SLOT_TARGET = 0;
    static constexpr uint32_t SLOT_PIPELINE = 1;
    static constexpr uint32_t SLOT_INDEX = 2;
    static constexpr uint32_t SLOT_VERTEX = 3;
    static constexpr uint32_t SLOT_COUNT = SLOT_VERTEX + RHI_MAX_VERTEX_SLOTS;

    // Held by pointer because the arena is not movable.
    UniquePtr<ArenaAllocator> m_arena;
    size_t m_block_size;
    SmallVector<Ref<RHIResource>, 16> m_retained;
    const RHIResource* m_last_in_slot[SLOT_COUNT] = {};
    RHIFormat m_target_format = RHIFormat::Undefined;
    RHICommand* m_head = nullptr;
    RHICommand* m_tail = nullptr;
    size_t m_count = 0;
    bool m_in_pass = false;
    bool m_has_pipeline = false;
    bool m_has_index_buffer = false;
};

} // namespace oryx
