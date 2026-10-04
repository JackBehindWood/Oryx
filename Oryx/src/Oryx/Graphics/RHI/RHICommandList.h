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

    void begin_pass(const RHIRenderPassDesc& pass);
    // Convenience: one colour attachment, cleared or loaded.
    void begin_pass(RHIRenderTarget* target, const RHIClear& clear = {});
    void set_pipeline(RHIGraphicsPipeline* pipeline);
    void set_viewport(const RHIViewportState& viewport);
    void set_scissor(const RHIScissorRect& scissor);
    void set_vertex_buffer(uint32_t slot, RHIBuffer* buffer, uint32_t offset = 0);
    void set_index_buffer(RHIBuffer* buffer, uint32_t offset = 0, bool index32 = true);
    // The bytes are copied into the list; at most RHI_MAX_CONSTANTS_SIZE and at most the binding's size.
    void set_constants(RHIBindingId binding, const void* data, uint32_t size);
    void bind_buffer(RHIBindingId binding, RHIBuffer* buffer, uint32_t offset, uint32_t size);
    void bind_texture(RHIBindingId binding, RHITexture* texture, uint32_t array_index = 0);
    void bind_sampler(RHIBindingId binding, RHISampler* sampler, uint32_t array_index = 0);
    void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0);
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t base_vertex = 0, uint32_t first_instance = 0);
    // Groups must be balanced inside a pass and outside passes separately.
    void push_debug_group(const char* name);
    void pop_debug_group();
    void end_pass();

    // Replays every command into the backend's context, in recording order.
    void execute(IRHICommandContext& context) const;

    // Drops all commands and releases the retained resources; the arena's memory is kept for reuse.
    void clear();

    // For backends: moves the retained resources into `sink` and clears the list.
    void drain_into(std::vector<Ref<RHIResource>>& sink);

    [[nodiscard]] Iterator begin() const { return Iterator(m_state.head); }
    [[nodiscard]] Iterator end() const { return Iterator(); }
    [[nodiscard]] size_t size() const { return m_state.count; }
    [[nodiscard]] bool empty() const { return m_state.count == 0; }
    [[nodiscard]] bool in_pass() const { return m_state.in_pass; }
    [[nodiscard]] uint32_t debug_depth() const { return m_state.debug_depth; }
    [[nodiscard]] size_t retained_count() const { return m_retained.size(); }

private:
    static constexpr uint32_t SLOT_TARGET = 0;
    static constexpr uint32_t SLOT_DEPTH = SLOT_TARGET + RHI_MAX_COLOUR_TARGETS;
    static constexpr uint32_t SLOT_PIPELINE = SLOT_DEPTH + 1;
    static constexpr uint32_t SLOT_INDEX = SLOT_PIPELINE + 1;
    static constexpr uint32_t SLOT_VERTEX = SLOT_INDEX + 1;
    static constexpr uint32_t SLOT_COUNT = SLOT_VERTEX + RHI_MAX_VERTEX_SLOTS;

    // Everything that resets with the recording; commands, caches and the current pass and pipeline.
    struct State
    {
        RHICommand* head = nullptr;
        RHICommand* tail = nullptr;
        size_t count = 0;
        const RHIGraphicsPipeline* pipeline = nullptr;
        RHIFormat pass_colour[RHI_MAX_COLOUR_TARGETS] = {};
        RHIFormat pass_depth = RHIFormat::Undefined;
        uint32_t pass_colour_count = 0;
        uint32_t pass_width = 0;
        uint32_t pass_height = 0;
        uint32_t debug_depth = 0;
        uint32_t pass_debug_base = 0;
        bool in_pass = false;
        bool has_index_buffer = false;
        const RHIResource* last_in_slot[SLOT_COUNT] = {};
        const RHIResource* last_binding[RHI_MAX_BINDINGS] = {};
    };

    template<typename T, typename... Args>
    void emplace(Args&&... args);

    [[nodiscard]] void* allocate_payload(size_t size, size_t alignment);
    void retain(RHIResource& resource);
    void retain_in_slot(uint32_t slot, RHIResource& resource);
    void retain_for_binding(RHIBindingId binding, RHIResource& resource);
    void require_pass(const char* what) const;
    void require_pipeline(const char* what) const;
    [[nodiscard]] const RHIBindingDesc& require_binding(RHIBindingId binding, const char* what) const;

    // Held by pointer because the arena is not movable.
    UniquePtr<ArenaAllocator> m_arena;
    size_t m_block_size = DEFAULT_BLOCK_SIZE;
    SmallVector<Ref<RHIResource>, 16> m_retained;
    State m_state;
};

} // namespace oryx
