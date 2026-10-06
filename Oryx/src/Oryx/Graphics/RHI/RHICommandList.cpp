#include "oxpch.h"
#include "Oryx/Graphics/RHI/RHICommandList.h"

#include "Oryx/Graphics/RHI/RHIViewport.h"

namespace oryx
{

RHICommandList::RHICommandList(RHICommandList&& other) noexcept
    : RHICommandListBase(std::move(other))
    , m_validation(other.m_validation)
{
    other.m_validation.reset();
}

RHICommandList& RHICommandList::operator=(RHICommandList&& other) noexcept
{
    if (this != &other)
    {
        RHICommandListBase::operator=(std::move(other));
        m_validation = other.m_validation;
        other.m_validation.reset();
    }
    return *this;
}

void RHICommandList::begin_pass(const RHIRenderPassDesc& pass)
{
    m_validation.begin_pass(pass);
    m_stream.emplace<RHIBeginPassCommand>(pass);
    for (uint32_t i = 0; i < pass.colour_count; ++i)
    {
        m_retainer.retain_in_slot(RHIResourceRetainer::SLOT_TARGET + i, *pass.colour[i].target);
    }
    if (pass.depth.texture != nullptr)
    {
        m_retainer.retain_in_slot(RHIResourceRetainer::SLOT_DEPTH, *pass.depth.texture);
    }
}

void RHICommandList::begin_pass(RHIRenderTarget* target, const RHIClear& clear)
{
    rhi_require_non_null(target, "begin_pass");
    RHIRenderPassDesc pass;
    pass.colour[0].target = target;
    pass.colour[0].load = clear.clear ? RHILoadAction::Clear : RHILoadAction::Load;
    pass.colour[0].clear_colour = clear.colour;
    pass.colour_count = 1;
    begin_pass(pass);
}

void RHICommandList::set_pipeline(RHIGraphicsPipeline* pipeline)
{
    rhi_require_non_null(pipeline, "set_pipeline");
    m_validation.set_pipeline(*pipeline);
    m_stream.emplace<RHISetPipelineCommand>(*pipeline);
    m_retainer.retain_in_slot(RHIResourceRetainer::SLOT_PIPELINE, *pipeline);
}

void RHICommandList::set_viewport(const RHIViewportState& viewport)
{
    m_validation.set_viewport(viewport);
    m_stream.emplace<RHISetViewportCommand>(viewport);
}

void RHICommandList::set_scissor(const RHIScissorRect& scissor)
{
    m_validation.set_scissor(scissor);
    m_stream.emplace<RHISetScissorCommand>(scissor);
}

void RHICommandList::set_vertex_buffer(uint32_t slot, RHIBuffer* buffer, uint32_t offset)
{
    rhi_require_non_null(buffer, "set_vertex_buffer");
    m_validation.set_vertex_buffer(slot, *buffer, offset);
    m_stream.emplace<RHISetVertexBufferCommand>(slot, *buffer, offset);
    m_retainer.retain_in_slot(RHIResourceRetainer::SLOT_VERTEX + slot, *buffer);
}

void RHICommandList::set_index_buffer(RHIBuffer* buffer, uint32_t offset, bool index32)
{
    rhi_require_non_null(buffer, "set_index_buffer");
    m_validation.set_index_buffer(*buffer, offset, index32);
    m_stream.emplace<RHISetIndexBufferCommand>(*buffer, offset, index32);
    m_retainer.retain_in_slot(RHIResourceRetainer::SLOT_INDEX, *buffer);
}

void RHICommandList::set_constants(RHIBindingId binding, const void* data, uint32_t size)
{
    m_validation.set_constants(binding, static_cast<const uint8_t*>(data), size);
    uint8_t* copy = static_cast<uint8_t*>(m_stream.allocate(size, 16));
    std::memcpy(copy, data, size);
    m_stream.emplace<RHISetConstantsCommand>(binding, copy, size);
}

void RHICommandList::bind_buffer(RHIBindingId binding, RHIBuffer* buffer, uint32_t offset, uint32_t size)
{
    rhi_require_non_null(buffer, "bind_buffer");
    m_validation.bind_buffer(binding, *buffer, offset, size);
    m_stream.emplace<RHIBindBufferCommand>(binding, *buffer, offset, size);
    m_retainer.retain_for_binding(binding, *buffer);
}

void RHICommandList::bind_texture(RHIBindingId binding, RHITexture* texture, uint32_t array_index)
{
    rhi_require_non_null(texture, "bind_texture");
    m_validation.bind_texture(binding, *texture, array_index);
    m_stream.emplace<RHIBindTextureCommand>(binding, *texture, array_index);
    m_retainer.retain_for_binding(binding, *texture);
}

void RHICommandList::bind_sampler(RHIBindingId binding, RHISampler* sampler, uint32_t array_index)
{
    rhi_require_non_null(sampler, "bind_sampler");
    m_validation.bind_sampler(binding, *sampler, array_index);
    m_stream.emplace<RHIBindSamplerCommand>(binding, *sampler, array_index);
    m_retainer.retain_for_binding(binding, *sampler);
}

void RHICommandList::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
    m_validation.draw(vertex_count, instance_count, first_vertex, first_instance);
    m_stream.emplace<RHIDrawCommand>(vertex_count, instance_count, first_vertex, first_instance);
}

void RHICommandList::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance)
{
    m_validation.draw_indexed(index_count, instance_count, first_index, base_vertex, first_instance);
    m_stream.emplace<RHIDrawIndexedCommand>(index_count, instance_count, first_index, base_vertex, first_instance);
}

void RHICommandList::push_debug_group(const char* name)
{
    rhi_require_non_null(name, "push_debug_group");
    const size_t length = std::strlen(name) + 1;
    char* copy = static_cast<char*>(m_stream.allocate(length, 1));
    std::memcpy(copy, name, length);
    m_validation.push_debug_group(copy);
    m_stream.emplace<RHIPushDebugGroupCommand>(copy);
}

void RHICommandList::pop_debug_group()
{
    m_validation.pop_debug_group();
    m_stream.emplace<RHIPopDebugGroupCommand>();
}

void RHICommandList::end_pass()
{
    m_validation.end_pass();
    m_stream.emplace<RHIEndPassCommand>();
}

void RHICommandList::copy_buffer(RHIBuffer* source, uint32_t source_offset, RHIBuffer* destination, uint32_t destination_offset, uint32_t size)
{
    rhi_require_non_null(source, "copy_buffer");
    rhi_require_non_null(destination, "copy_buffer");
    m_validation.copy_buffer(*source, source_offset, *destination, destination_offset, size);
    m_stream.emplace<RHICopyBufferCommand>(*source, source_offset, *destination, destination_offset, size);
    m_retainer.retain(*source);
    m_retainer.retain(*destination);
}

void RHICommandList::clear()
{
    RHICommandListBase::clear();
    m_validation.reset();
}

void RHICommandList::drain_into(std::vector<Ref<RHIResource>>& sink)
{
    RHICommandListBase::drain_into(sink);
    m_validation.reset();
}

} // namespace oryx
