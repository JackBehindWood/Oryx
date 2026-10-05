#include "oxpch.h"
#include "Oryx/Graphics/RHI/RHICommandList.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

constexpr size_t RETAIN_SCAN_WINDOW = 16;

void fail(const char* message, const char* what)
{
    throw Error(std::string("RHICommandList: ") + message + " in " + what);
}

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
        fail("offset out of range", what);
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
    , m_state(other.m_state)
{
    other.m_retained.clear();
    other.m_state = State{};
}

RHICommandList& RHICommandList::operator=(RHICommandList&& other) noexcept
{
    if (this != &other)
    {
        m_arena = std::move(other.m_arena);
        m_block_size = other.m_block_size;
        m_retained = std::move(other.m_retained);
        m_state = other.m_state;
        other.m_retained.clear();
        other.m_state = State{};
    }
    return *this;
}

template<typename T, typename... Args>
void RHICommandList::emplace(Args&&... args)
{
    static_assert(std::is_base_of_v<RHICommand, T>);
    static_assert(std::is_trivially_destructible_v<T>, "commands are never destroyed individually");
    void* memory = allocate_payload(sizeof(T), alignof(T));
    T* command = new (memory) T(std::forward<Args>(args)...);
    if (m_state.tail != nullptr)
    {
        m_state.tail->m_next = command;
    }
    else
    {
        m_state.head = command;
    }
    m_state.tail = command;
    ++m_state.count;
}

void* RHICommandList::allocate_payload(size_t size, size_t alignment)
{
    if (!m_arena)
    {
        m_arena = create_unique<ArenaAllocator>(detail::object_allocator(), m_block_size);
    }
    return m_arena->allocate(size, alignment);
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
    if (m_state.last_in_slot[slot] == &resource)
    {
        return;
    }
    retain(resource);
    m_state.last_in_slot[slot] = &resource;
}

void RHICommandList::retain_for_binding(RHIBindingId binding, RHIResource& resource)
{
    if (m_state.last_binding[binding] == &resource)
    {
        return;
    }
    retain(resource);
    m_state.last_binding[binding] = &resource;
}

void RHICommandList::require_pass(const char* what) const
{
    if (!m_state.in_pass)
    {
        throw Error(std::string("RHICommandList: ") + what + " outside a pass");
    }
}

void RHICommandList::require_pipeline(const char* what) const
{
    require_pass(what);
    if (m_state.pipeline == nullptr)
    {
        throw Error(std::string("RHICommandList: ") + what + " without a pipeline");
    }
}

const RHIBindingDesc& RHICommandList::require_binding(RHIBindingId binding, const char* what) const
{
    require_pipeline(what);
    return m_state.pipeline->binding(binding);
}

void RHICommandList::begin_pass(const RHIRenderPassDesc& pass)
{
    if (m_state.in_pass)
    {
        throw Error("RHICommandList: begin_pass inside a pass");
    }
    if (pass.colour_count > RHI_MAX_COLOUR_TARGETS)
    {
        throw Error("RHICommandList: begin_pass has too many colour attachments");
    }
    if (pass.colour_count == 0 && pass.depth.texture == nullptr)
    {
        throw Error("RHICommandList: begin_pass has no attachments");
    }

    RHIFormat colour_formats[RHI_MAX_COLOUR_TARGETS] = {};
    RHIFormat depth_format = RHIFormat::Undefined;
    uint32_t width = 0;
    uint32_t height = 0;
    for (uint32_t i = 0; i < pass.colour_count; ++i)
    {
        const RHIRenderTarget* target = pass.colour[i].target;
        require_non_null(target, "begin_pass");
        if (!rhi_format_is_colour(target->format()))
        {
            throw Error("RHICommandList: begin_pass target is not a colour format");
        }
        if (i > 0 && (target->width() != width || target->height() != height))
        {
            throw Error("RHICommandList: begin_pass attachments differ in size");
        }
        width = target->width();
        height = target->height();
        colour_formats[i] = target->format();
    }
    if (const RHITexture* depth = pass.depth.texture)
    {
        if (!has_flag(depth->usage(), RHITextureUsage::DepthStencil) || !rhi_format_is_depth(depth->format()))
        {
            throw Error("RHICommandList: begin_pass depth attachment is not a depth-stencil texture");
        }
        if (depth->sample_count() != 1 || depth->dimension() != RHITextureDimension::Tex2D)
        {
            throw Error("RHICommandList: begin_pass depth attachment must be a single-sample 2D texture");
        }
        if (pass.colour_count > 0 && (depth->width() != width || depth->height() != height))
        {
            throw Error("RHICommandList: begin_pass attachments differ in size");
        }
        width = depth->width();
        height = depth->height();
        depth_format = depth->format();
    }

    emplace<RHIBeginPassCommand>(pass);
    for (uint32_t i = 0; i < pass.colour_count; ++i)
    {
        retain_in_slot(SLOT_TARGET + i, *pass.colour[i].target);
    }
    if (pass.depth.texture != nullptr)
    {
        retain_in_slot(SLOT_DEPTH, *pass.depth.texture);
    }
    std::memcpy(m_state.pass_colour, colour_formats, sizeof(colour_formats));
    m_state.pass_colour_count = pass.colour_count;
    m_state.pass_depth = depth_format;
    m_state.pass_width = width;
    m_state.pass_height = height;
    m_state.pass_debug_base = m_state.debug_depth;
    m_state.pipeline = nullptr;
    m_state.has_index_buffer = false;
    m_state.in_pass = true;
}

void RHICommandList::begin_pass(RHIRenderTarget* target, const RHIClear& clear)
{
    require_non_null(target, "begin_pass");
    RHIRenderPassDesc pass;
    pass.colour[0].target = target;
    pass.colour[0].load = clear.clear ? RHILoadAction::Clear : RHILoadAction::Load;
    pass.colour[0].clear_colour = clear.colour;
    pass.colour_count = 1;
    begin_pass(pass);
}

void RHICommandList::set_pipeline(RHIGraphicsPipeline* pipeline)
{
    require_pass("set_pipeline");
    require_non_null(pipeline, "set_pipeline");
    bool compatible = pipeline->colour_format_count() == m_state.pass_colour_count && pipeline->depth_format() == m_state.pass_depth;
    for (uint32_t i = 0; compatible && i < m_state.pass_colour_count; ++i)
    {
        compatible = pipeline->colour_formats()[i] == m_state.pass_colour[i];
    }
    if (!compatible)
    {
        throw Error("RHICommandList: pipeline colour or depth format does not match the pass attachments");
    }
    if (pipeline->sample_count() != 1)
    {
        throw Error("RHICommandList: pipeline sample count does not match the pass");
    }
    emplace<RHISetPipelineCommand>(*pipeline);
    retain_in_slot(SLOT_PIPELINE, *pipeline);
    m_state.pipeline = pipeline;
}

void RHICommandList::set_viewport(const RHIViewportState& viewport)
{
    require_pass("set_viewport");
    if (!(viewport.width > 0.0f) || !(viewport.height > 0.0f))
    {
        fail("viewport size must be positive", "set_viewport");
    }
    if (!(viewport.min_depth >= 0.0f) || !(viewport.max_depth <= 1.0f) || viewport.min_depth > viewport.max_depth)
    {
        fail("viewport depth range must lie within [0, 1]", "set_viewport");
    }
    emplace<RHISetViewportCommand>(viewport);
}

void RHICommandList::set_scissor(const RHIScissorRect& scissor)
{
    require_pass("set_scissor");
    const int64_t right = static_cast<int64_t>(scissor.x) + scissor.width;
    const int64_t bottom = static_cast<int64_t>(scissor.y) + scissor.height;
    if (scissor.x < 0 || scissor.y < 0 || right > m_state.pass_width || bottom > m_state.pass_height)
    {
        fail("scissor lies outside the pass attachments", "set_scissor");
    }
    emplace<RHISetScissorCommand>(scissor);
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
    m_state.has_index_buffer = true;
}

void RHICommandList::set_constants(RHIBindingId binding, const void* data, uint32_t size)
{
    const RHIBindingDesc& desc = require_binding(binding, "set_constants");
    if (desc.kind != RHIBindingKind::Constants)
    {
        fail("binding is not a constants binding", "set_constants");
    }
    if (data == nullptr || size == 0)
    {
        fail("constants need data", "set_constants");
    }
    if (size > RHI_MAX_CONSTANTS_SIZE || size > desc.size)
    {
        fail("constants exceed the binding size", "set_constants");
    }
    uint8_t* copy = static_cast<uint8_t*>(allocate_payload(size, 16));
    std::memcpy(copy, data, size);
    emplace<RHISetConstantsCommand>(binding, copy, size);
}

void RHICommandList::bind_buffer(RHIBindingId binding, RHIBuffer* buffer, uint32_t offset, uint32_t size)
{
    const RHIBindingDesc& desc = require_binding(binding, "bind_buffer");
    require_non_null(buffer, "bind_buffer");
    if (!rhi_binding_is_buffer(desc.kind))
    {
        fail("binding is not a buffer binding", "bind_buffer");
    }
    require_usage(buffer->usage(), desc.kind == RHIBindingKind::UniformBuffer ? RHIBufferUsage::Uniform : RHIBufferUsage::Storage, "bind_buffer");
    if (size == 0 || static_cast<uint64_t>(offset) + size > buffer->size())
    {
        fail("range out of the buffer", "bind_buffer");
    }
    if (desc.size != 0 && size > desc.size)
    {
        fail("range exceeds the binding size", "bind_buffer");
    }
    emplace<RHIBindBufferCommand>(binding, *buffer, offset, size);
    retain_for_binding(binding, *buffer);
}

void RHICommandList::bind_texture(RHIBindingId binding, RHITexture* texture, uint32_t array_index)
{
    const RHIBindingDesc& desc = require_binding(binding, "bind_texture");
    require_non_null(texture, "bind_texture");
    if (desc.kind != RHIBindingKind::SampledTexture)
    {
        fail("binding is not a sampled texture binding", "bind_texture");
    }
    if (array_index >= desc.array_count)
    {
        fail("array index out of range", "bind_texture");
    }
    if (!has_flag(texture->usage(), RHITextureUsage::Sampled))
    {
        throw Error("RHICommandList: bound texture was not created with RHITextureUsage::Sampled");
    }
    if (texture->dimension() != desc.texture_dimension)
    {
        fail("texture dimension does not match the binding", "bind_texture");
    }
    if (rhi_format_data_type(texture->format()) != desc.data_type)
    {
        fail("texture sample type does not match the binding", "bind_texture");
    }
    emplace<RHIBindTextureCommand>(binding, *texture, array_index);
    retain_for_binding(binding, *texture);
}

void RHICommandList::bind_sampler(RHIBindingId binding, RHISampler* sampler, uint32_t array_index)
{
    const RHIBindingDesc& desc = require_binding(binding, "bind_sampler");
    require_non_null(sampler, "bind_sampler");
    if (desc.kind != RHIBindingKind::Sampler)
    {
        fail("binding is not a sampler binding", "bind_sampler");
    }
    if (array_index >= desc.array_count)
    {
        fail("array index out of range", "bind_sampler");
    }
    emplace<RHIBindSamplerCommand>(binding, *sampler, array_index);
    retain_for_binding(binding, *sampler);
}

void RHICommandList::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
    require_pipeline("draw");
    emplace<RHIDrawCommand>(vertex_count, instance_count, first_vertex, first_instance);
}

void RHICommandList::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance)
{
    require_pipeline("draw_indexed");
    if (!m_state.has_index_buffer)
    {
        throw Error("RHICommandList: draw_indexed without an index buffer");
    }
    emplace<RHIDrawIndexedCommand>(index_count, instance_count, first_index, base_vertex, first_instance);
}

void RHICommandList::push_debug_group(const char* name)
{
    require_non_null(name, "push_debug_group");
    const size_t length = std::strlen(name) + 1;
    char* copy = static_cast<char*>(allocate_payload(length, 1));
    std::memcpy(copy, name, length);
    emplace<RHIPushDebugGroupCommand>(copy);
    ++m_state.debug_depth;
}

void RHICommandList::pop_debug_group()
{
    const uint32_t floor = m_state.in_pass ? m_state.pass_debug_base : 0;
    if (m_state.debug_depth <= floor)
    {
        throw Error("RHICommandList: pop_debug_group without a matching push");
    }
    emplace<RHIPopDebugGroupCommand>();
    --m_state.debug_depth;
}

void RHICommandList::end_pass()
{
    require_pass("end_pass");
    if (m_state.debug_depth != m_state.pass_debug_base)
    {
        throw Error("RHICommandList: end_pass with an unbalanced debug group");
    }
    emplace<RHIEndPassCommand>();
    m_state.in_pass = false;
}

void RHICommandList::copy_buffer(RHIBuffer* source, uint32_t source_offset, RHIBuffer* destination, uint32_t destination_offset, uint32_t size)
{
    if (m_state.in_pass)
    {
        fail("a copy is not allowed inside a pass", "copy_buffer");
    }
    require_non_null(source, "copy_buffer");
    require_non_null(destination, "copy_buffer");
    if (source == destination)
    {
        fail("source and destination are the same buffer", "copy_buffer");
    }
    require_usage(source->usage(), RHIBufferUsage::CopySource, "copy_buffer source");
    require_usage(destination->usage(), RHIBufferUsage::CopyDest, "copy_buffer destination");
    if (size == 0 || source_offset > source->size() || size > source->size() - source_offset || destination_offset > destination->size() || size > destination->size() - destination_offset)
    {
        fail("range out of bounds", "copy_buffer");
    }
    emplace<RHICopyBufferCommand>(*source, source_offset, *destination, destination_offset, size);
    retain(*source);
    retain(*destination);
}

void RHICommandList::execute(IRHICommandContext& context) const
{
    for (const RHICommand* command = m_state.head; command != nullptr; command = command->next())
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
    m_state = State{};
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
