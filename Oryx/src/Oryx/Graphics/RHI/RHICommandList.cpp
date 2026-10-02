#include "oxpch.h"
#include "Oryx/Graphics/RHI/RHICommandList.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

constexpr size_t RETAIN_SCAN_WINDOW = 8;

void require_non_null(const void* resource, const char* what)
{
    if (resource == nullptr)
    {
        throw Error(std::string("RHICommandList: null resource passed to ") + what);
    }
}

void require_range(uint32_t offset, uint32_t size, const char* what)
{
    if (offset > size)
    {
        throw Error(std::string("RHICommandList: offset out of range in ") + what);
    }
}

void require_usage(RHIBufferUsage usage, RHIBufferUsage required, const char* what)
{
    if (!has_flag(usage, required))
    {
        throw Error(std::string("RHICommandList: buffer lacks the required usage for ") + what);
    }
}

} // namespace

RHICommandList::RHICommandList(RHICommandList&& other) noexcept
    : m_arena(std::move(other.m_arena))
    , m_block_size(other.m_block_size)
    , m_retained(std::move(other.m_retained))
    , m_target_format(std::exchange(other.m_target_format, RHIFormat::Undefined))
    , m_head(std::exchange(other.m_head, nullptr))
    , m_tail(std::exchange(other.m_tail, nullptr))
    , m_count(std::exchange(other.m_count, 0))
    , m_in_pass(std::exchange(other.m_in_pass, false))
    , m_has_pipeline(std::exchange(other.m_has_pipeline, false))
    , m_has_index_buffer(std::exchange(other.m_has_index_buffer, false))
{
    std::memcpy(m_last_in_slot, other.m_last_in_slot, sizeof(m_last_in_slot));
    other.m_retained.clear();
    std::memset(other.m_last_in_slot, 0, sizeof(other.m_last_in_slot));
}

RHICommandList& RHICommandList::operator=(RHICommandList&& other) noexcept
{
    if (this != &other)
    {
        m_arena = std::move(other.m_arena);
        m_block_size = other.m_block_size;
        m_retained = std::move(other.m_retained);
        other.m_retained.clear();
        m_target_format = std::exchange(other.m_target_format, RHIFormat::Undefined);
        std::memcpy(m_last_in_slot, other.m_last_in_slot, sizeof(m_last_in_slot));
        std::memset(other.m_last_in_slot, 0, sizeof(other.m_last_in_slot));
        m_head = std::exchange(other.m_head, nullptr);
        m_tail = std::exchange(other.m_tail, nullptr);
        m_count = std::exchange(other.m_count, 0);
        m_in_pass = std::exchange(other.m_in_pass, false);
        m_has_pipeline = std::exchange(other.m_has_pipeline, false);
        m_has_index_buffer = std::exchange(other.m_has_index_buffer, false);
    }
    return *this;
}

template<typename T, typename... Args>
void RHICommandList::emplace(Args&&... args)
{
    static_assert(std::is_base_of_v<RHICommand, T>);
    static_assert(std::is_trivially_destructible_v<T>, "commands are never destroyed individually");
    if (!m_arena)
    {
        m_arena = create_unique<ArenaAllocator>(detail::object_allocator(), m_block_size);
    }
    void* memory = m_arena->allocate(sizeof(T), alignof(T));
    T* command = new (memory) T(std::forward<Args>(args)...);
    if (m_tail != nullptr)
    {
        m_tail->m_next = command;
    }
    else
    {
        m_head = command;
    }
    m_tail = command;
    ++m_count;
}

void RHICommandList::retain(RHIResource& resource)
{
    const size_t count = m_retained.size();
    const size_t window = std::min(count, RETAIN_SCAN_WINDOW);
    for (size_t i = count - window; i < count; ++i)
    {
        if (m_retained[i].get() == &resource)
        {
            return;
        }
    }
    m_retained.push_back(Ref<RHIResource>::from_raw(&resource));
}

void RHICommandList::retain_in_slot(uint32_t slot, RHIResource& resource)
{
    if (m_last_in_slot[slot] == &resource)
    {
        return;
    }
    retain(resource);
    m_last_in_slot[slot] = &resource;
}

void RHICommandList::require_pass(const char* what) const
{
    if (!m_in_pass)
    {
        throw Error(std::string("RHICommandList: ") + what + " outside a pass");
    }
}

void RHICommandList::require_pipeline(const char* what) const
{
    require_pass(what);
    if (!m_has_pipeline)
    {
        throw Error(std::string("RHICommandList: ") + what + " without a pipeline");
    }
}

void RHICommandList::begin_pass(RHIRenderTarget* target, const RHIClear& clear)
{
    if (m_in_pass)
    {
        throw Error("RHICommandList: begin_pass inside a pass");
    }
    require_non_null(target, "begin_pass");
    if (!rhi_format_is_colour(target->format()))
    {
        throw Error("RHICommandList: begin_pass target is not a colour format");
    }
    emplace<RHIBeginPassCommand>(*target, clear);
    retain_in_slot(SLOT_TARGET, *target);
    m_target_format = target->format();
    m_in_pass = true;
    m_has_pipeline = false;
    m_has_index_buffer = false;
}

void RHICommandList::set_pipeline(RHIGraphicsPipeline* pipeline)
{
    require_pass("set_pipeline");
    require_non_null(pipeline, "set_pipeline");
    if (pipeline->colour_format() != m_target_format)
    {
        throw Error("RHICommandList: pipeline colour format does not match the pass target");
    }
    emplace<RHISetPipelineCommand>(*pipeline);
    retain_in_slot(SLOT_PIPELINE, *pipeline);
    m_has_pipeline = true;
}

void RHICommandList::set_vertex_buffer(uint32_t slot, RHIBuffer* buffer, uint32_t offset)
{
    require_pass("set_vertex_buffer");
    require_non_null(buffer, "set_vertex_buffer");
    if (slot >= RHI_MAX_VERTEX_SLOTS)
    {
        throw Error("RHICommandList: vertex buffer slot out of range");
    }
    require_usage(buffer->usage(), RHIBufferUsage::Vertex, "set_vertex_buffer");
    require_range(offset, buffer->size(), "set_vertex_buffer");
    emplace<RHISetVertexBufferCommand>(slot, *buffer, offset);
    retain_in_slot(SLOT_VERTEX + slot, *buffer);
}

void RHICommandList::set_index_buffer(RHIBuffer* buffer, uint32_t offset, bool index32)
{
    require_pass("set_index_buffer");
    require_non_null(buffer, "set_index_buffer");
    require_usage(buffer->usage(), RHIBufferUsage::Index, "set_index_buffer");
    require_range(offset, buffer->size(), "set_index_buffer");
    emplace<RHISetIndexBufferCommand>(*buffer, offset, index32);
    retain_in_slot(SLOT_INDEX, *buffer);
    m_has_index_buffer = true;
}

void RHICommandList::bind(RHIBindingId binding, RHIBuffer* buffer)
{
    require_pipeline("bind");
    require_non_null(buffer, "bind");
    require_usage(buffer->usage(), RHIBufferUsage::Uniform, "bind");
    emplace<RHIBindBufferCommand>(binding, *buffer);
    retain(*buffer);
}

void RHICommandList::bind(RHIBindingId binding, RHITexture* texture)
{
    require_pipeline("bind");
    require_non_null(texture, "bind");
    if (!has_flag(texture->usage(), RHITextureUsage::Sampled))
    {
        throw Error("RHICommandList: bound texture was not created with RHITextureUsage::Sampled");
    }
    emplace<RHIBindTextureCommand>(binding, *texture);
    retain(*texture);
}

void RHICommandList::bind(RHIBindingId binding, RHISampler* sampler)
{
    require_pipeline("bind");
    require_non_null(sampler, "bind");
    emplace<RHIBindSamplerCommand>(binding, *sampler);
    retain(*sampler);
}

void RHICommandList::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex)
{
    require_pipeline("draw");
    emplace<RHIDrawCommand>(vertex_count, instance_count, first_vertex);
}

void RHICommandList::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index)
{
    require_pipeline("draw_indexed");
    if (!m_has_index_buffer)
    {
        throw Error("RHICommandList: draw_indexed without an index buffer");
    }
    emplace<RHIDrawIndexedCommand>(index_count, instance_count, first_index);
}

void RHICommandList::end_pass()
{
    require_pass("end_pass");
    emplace<RHIEndPassCommand>();
    m_in_pass = false;
}

void RHICommandList::execute(IRHICommandContext& context) const
{
    for (const RHICommand* command = m_head; command != nullptr; command = command->next())
    {
        command->execute(context);
    }
}

void RHICommandList::clear()
{
    if (m_arena)
    {
        m_arena->reset();
    }
    m_retained.clear();
    std::memset(m_last_in_slot, 0, sizeof(m_last_in_slot));
    m_head = nullptr;
    m_tail = nullptr;
    m_count = 0;
    m_in_pass = false;
    m_has_pipeline = false;
    m_has_index_buffer = false;
    m_target_format = RHIFormat::Undefined;
}

void RHICommandList::drain_into(std::vector<Ref<RHIResource>>& sink)
{
    sink.reserve(sink.size() + m_retained.size());
    for (Ref<RHIResource>& resource : m_retained)
    {
        sink.push_back(std::move(resource));
    }
    clear();
}

} // namespace oryx
