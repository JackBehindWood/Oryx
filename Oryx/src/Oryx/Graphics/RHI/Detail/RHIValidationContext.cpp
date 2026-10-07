#include "oxpch.h"
#include "Oryx/Graphics/RHI/Detail/RHIValidationContext.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/RHIViewport.h"

namespace oryx
{

namespace
{

uint32_t required_elements(const RHIBindingDesc& desc)
{
    return desc.array_count >= 32 ? 0xFFFFFFFFu : (1u << desc.array_count) - 1u;
}

} // namespace

std::string RHIValidationContext::breadcrumb_path() const
{
    std::string path;
    const uint32_t named = std::min(m_debug_depth, MAX_BREADCRUMBS);
    for (uint32_t i = 0; i < named; ++i)
    {
        path += i == 0 ? "" : " > ";
        path += m_breadcrumbs[i];
    }
    return path;
}

void RHIValidationContext::fail(const char* what, const std::string& message) const
{
    const std::string path = breadcrumb_path();
    throw Error("RHI validation" + (path.empty() ? std::string() : " [" + path + "]") + ": " + message + " in " + what);
}

void RHIValidationContext::reset()
{
    m_pipeline = nullptr;
    std::memset(m_pass_colour, 0, sizeof(m_pass_colour));
    m_pass_depth = RHIFormat::Undefined;
    m_pass_colour_count = 0;
    m_pass_width = 0;
    m_pass_height = 0;
    m_debug_depth = 0;
    m_pass_debug_base = 0;
    m_in_pass = false;
    m_has_index_buffer = false;
    m_index32 = true;
    m_index_offset = 0;
    m_index_buffer_size = 0;
    m_vertex_slots_bound = 0;
    std::memset(m_bound_elements, 0, sizeof(m_bound_elements));
    std::memset(m_breadcrumbs, 0, sizeof(m_breadcrumbs));
}

void RHIValidationContext::require_pass(const char* what) const
{
    if (!m_in_pass)
    {
        fail(what, "call outside a pass");
    }
}

void RHIValidationContext::require_pipeline(const char* what) const
{
    require_pass(what);
    if (m_pipeline == nullptr)
    {
        fail(what, "call without a pipeline");
    }
}

const RHIBindingDesc& RHIValidationContext::require_binding(RHIBindingId binding, const char* what) const
{
    require_pipeline(what);
    return m_pipeline->binding(binding);
}

void RHIValidationContext::mark_bound(RHIBindingId binding, uint32_t array_index)
{
    m_bound_elements[binding] |= 1u << array_index;
}

void RHIValidationContext::require_draw_ready(const char* what) const
{
    require_pipeline(what);
    const uint32_t missing_slots = m_pipeline->vertex_slot_mask() & ~m_vertex_slots_bound;
    if (missing_slots != 0)
    {
        fail(what, "vertex slot " + std::to_string(std::countr_zero(missing_slots)) + " is required by the pipeline but has no vertex buffer");
    }
    for (uint32_t id = 0; id < m_pipeline->binding_count(); ++id)
    {
        const RHIBindingDesc& desc = m_pipeline->bindings()[id];
        if (desc.kind == RHIBindingKind::StorageTexture)
        {
            continue;
        }
        const uint32_t missing = required_elements(desc) & ~m_bound_elements[id];
        if (missing != 0)
        {
            fail(what, "binding " + std::to_string(id) + " is not bound (element " + std::to_string(std::countr_zero(missing)) + ")");
        }
    }
}

void RHIValidationContext::begin_pass(const RHIRenderPassDesc& pass)
{
    if (m_in_pass)
    {
        fail("begin_pass", "call inside a pass");
    }
    if (pass.colour_count > RHI_MAX_COLOUR_TARGETS)
    {
        fail("begin_pass", "too many colour attachments");
    }
    if (pass.colour_count == 0 && pass.depth.texture == nullptr)
    {
        fail("begin_pass", "no attachments");
    }

    RHIFormat colour_formats[RHI_MAX_COLOUR_TARGETS] = {};
    RHIFormat depth_format = RHIFormat::Undefined;
    uint32_t width = 0;
    uint32_t height = 0;
    for (uint32_t i = 0; i < pass.colour_count; ++i)
    {
        const RHIRenderTarget* target = pass.colour[i].target;
        rhi_require_non_null(target, "begin_pass");
        if (!rhi_format_is_colour(target->format()))
        {
            fail("begin_pass", "target is not a colour format");
        }
        if (i > 0 && (target->width() != width || target->height() != height))
        {
            fail("begin_pass", "attachments differ in size");
        }
        width = target->width();
        height = target->height();
        colour_formats[i] = target->format();
    }
    if (const RHITexture* depth = pass.depth.texture)
    {
        if (!has_flag(depth->usage(), RHITextureUsage::DepthStencil) || !rhi_format_is_depth(depth->format()))
        {
            fail("begin_pass", "depth attachment is not a depth-stencil texture");
        }
        if (depth->sample_count() != 1 || depth->dimension() != RHITextureDimension::Tex2D)
        {
            fail("begin_pass", "depth attachment must be a single-sample 2D texture");
        }
        if (pass.colour_count > 0 && (depth->width() != width || depth->height() != height))
        {
            fail("begin_pass", "attachments differ in size");
        }
        width = depth->width();
        height = depth->height();
        depth_format = depth->format();
    }

    if (m_inner != nullptr)
    {
        m_inner->begin_pass(pass);
    }
    std::memcpy(m_pass_colour, colour_formats, sizeof(colour_formats));
    m_pass_colour_count = pass.colour_count;
    m_pass_depth = depth_format;
    m_pass_width = width;
    m_pass_height = height;
    m_pass_debug_base = m_debug_depth;
    m_pipeline = nullptr;
    m_has_index_buffer = false;
    m_vertex_slots_bound = 0;
    std::memset(m_bound_elements, 0, sizeof(m_bound_elements));
    m_in_pass = true;
}

void RHIValidationContext::set_pipeline(RHIGraphicsPipeline& pipeline)
{
    require_pass("set_pipeline");
    bool compatible = pipeline.colour_format_count() == m_pass_colour_count && pipeline.depth_format() == m_pass_depth;
    for (uint32_t i = 0; compatible && i < m_pass_colour_count; ++i)
    {
        compatible = pipeline.colour_formats()[i] == m_pass_colour[i];
    }
    if (!compatible)
    {
        fail("set_pipeline", "pipeline colour or depth format does not match the pass attachments");
    }
    if (pipeline.sample_count() != 1)
    {
        fail("set_pipeline", "pipeline sample count does not match the pass");
    }
    if (m_inner != nullptr)
    {
        m_inner->set_pipeline(pipeline);
    }
    m_pipeline = &pipeline;
    m_vertex_slots_bound = 0;
    std::memset(m_bound_elements, 0, sizeof(m_bound_elements));
}

void RHIValidationContext::set_viewport(const RHIViewportState& viewport)
{
    require_pass("set_viewport");
    if (!(viewport.width > 0.0f) || !(viewport.height > 0.0f))
    {
        fail("set_viewport", "viewport size must be positive");
    }
    if (!(viewport.min_depth >= 0.0f) || !(viewport.max_depth <= 1.0f) || viewport.min_depth > viewport.max_depth)
    {
        fail("set_viewport", "viewport depth range must lie within [0, 1]");
    }
    if (m_inner != nullptr)
    {
        m_inner->set_viewport(viewport);
    }
}

void RHIValidationContext::set_scissor(const RHIScissorRect& scissor)
{
    require_pass("set_scissor");
    const int64_t right = static_cast<int64_t>(scissor.x) + scissor.width;
    const int64_t bottom = static_cast<int64_t>(scissor.y) + scissor.height;
    if (scissor.width == 0 || scissor.height == 0)
    {
        fail("set_scissor", "scissor has no area; cull the draw instead");
    }
    if (scissor.x < 0 || scissor.y < 0 || right > m_pass_width || bottom > m_pass_height)
    {
        fail("set_scissor", "scissor lies outside the pass attachments");
    }
    if (m_inner != nullptr)
    {
        m_inner->set_scissor(scissor);
    }
}

void RHIValidationContext::set_vertex_buffer(uint32_t slot, RHIBuffer& buffer, uint32_t offset)
{
    require_pass("set_vertex_buffer");
    if (slot >= RHI_MAX_VERTEX_SLOTS)
    {
        fail("set_vertex_buffer", "vertex buffer slot out of range");
    }
    if (!has_flag(buffer.usage(), RHIBufferUsage::Vertex))
    {
        fail("set_vertex_buffer", "buffer lacks the required usage");
    }
    if (offset > buffer.size())
    {
        fail("set_vertex_buffer", "offset out of range");
    }
    if (m_inner != nullptr)
    {
        m_inner->set_vertex_buffer(slot, buffer, offset);
    }
    m_vertex_slots_bound |= 1u << slot;
}

void RHIValidationContext::set_index_buffer(RHIBuffer& buffer, uint32_t offset, bool index32)
{
    require_pass("set_index_buffer");
    if (!has_flag(buffer.usage(), RHIBufferUsage::Index))
    {
        fail("set_index_buffer", "buffer lacks the required usage");
    }
    if (offset > buffer.size())
    {
        fail("set_index_buffer", "offset out of range");
    }
    if (m_inner != nullptr)
    {
        m_inner->set_index_buffer(buffer, offset, index32);
    }
    m_has_index_buffer = true;
    m_index32 = index32;
    m_index_offset = offset;
    m_index_buffer_size = buffer.size();
}

void RHIValidationContext::set_constants(RHIBindingId binding, const uint8_t* data, uint32_t size)
{
    const RHIBindingDesc& desc = require_binding(binding, "set_constants");
    if (desc.kind != RHIBindingKind::Constants)
    {
        fail("set_constants", "binding is not a constants binding");
    }
    if (data == nullptr || size == 0)
    {
        fail("set_constants", "constants need data");
    }
    if (size > RHI_MAX_CONSTANTS_SIZE || size > desc.size)
    {
        fail("set_constants", "constants exceed the binding size");
    }
    if (m_inner != nullptr)
    {
        m_inner->set_constants(binding, data, size);
    }
    mark_bound(binding, 0);
}

void RHIValidationContext::bind_buffer(RHIBindingId binding, RHIBuffer& buffer, uint32_t offset, uint32_t size)
{
    const RHIBindingDesc& desc = require_binding(binding, "bind_buffer");
    if (!rhi_binding_is_buffer(desc.kind))
    {
        fail("bind_buffer", "binding is not a buffer binding");
    }
    if (!has_flag(buffer.usage(), desc.kind == RHIBindingKind::UniformBuffer ? RHIBufferUsage::Uniform : RHIBufferUsage::Storage))
    {
        fail("bind_buffer", "buffer lacks the required usage");
    }
    if (size == 0 || static_cast<uint64_t>(offset) + size > buffer.size())
    {
        fail("bind_buffer", "range out of the buffer");
    }
    if (desc.size != 0 && size > desc.size)
    {
        fail("bind_buffer", "range exceeds the binding size");
    }
    if (m_inner != nullptr)
    {
        m_inner->bind_buffer(binding, buffer, offset, size);
    }
    mark_bound(binding, 0);
}

void RHIValidationContext::bind_texture(RHIBindingId binding, RHITexture& texture, uint32_t array_index)
{
    const RHIBindingDesc& desc = require_binding(binding, "bind_texture");
    if (desc.kind != RHIBindingKind::SampledTexture)
    {
        fail("bind_texture", "binding is not a sampled texture binding");
    }
    if (array_index >= desc.array_count)
    {
        fail("bind_texture", "array index out of range");
    }
    if (!has_flag(texture.usage(), RHITextureUsage::Sampled))
    {
        fail("bind_texture", "bound texture was not created with RHITextureUsage::Sampled");
    }
    if (texture.dimension() != desc.texture_dimension)
    {
        fail("bind_texture", "texture dimension does not match the binding");
    }
    if (rhi_format_data_type(texture.format()) != desc.data_type)
    {
        fail("bind_texture", "texture sample type does not match the binding");
    }
    if (m_inner != nullptr)
    {
        m_inner->bind_texture(binding, texture, array_index);
    }
    mark_bound(binding, array_index);
}

void RHIValidationContext::bind_sampler(RHIBindingId binding, RHISampler& sampler, uint32_t array_index)
{
    const RHIBindingDesc& desc = require_binding(binding, "bind_sampler");
    if (desc.kind != RHIBindingKind::Sampler)
    {
        fail("bind_sampler", "binding is not a sampler binding");
    }
    if (array_index >= desc.array_count)
    {
        fail("bind_sampler", "array index out of range");
    }
    if (m_inner != nullptr)
    {
        m_inner->bind_sampler(binding, sampler, array_index);
    }
    mark_bound(binding, array_index);
}

void RHIValidationContext::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
    require_draw_ready("draw");
    if (m_inner != nullptr)
    {
        m_inner->draw(vertex_count, instance_count, first_vertex, first_instance);
    }
}

void RHIValidationContext::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance)
{
    require_draw_ready("draw_indexed");
    if (!m_has_index_buffer)
    {
        fail("draw_indexed", "call without an index buffer");
    }
    const uint64_t index_bytes = m_index32 ? 4 : 2;
    const uint64_t end = m_index_offset + (static_cast<uint64_t>(first_index) + index_count) * index_bytes;
    if (end > m_index_buffer_size)
    {
        fail("draw_indexed", "indices reach byte " + std::to_string(end) + " of a " + std::to_string(m_index_buffer_size) + " byte index buffer (first " + std::to_string(first_index) + ", count " + std::to_string(index_count) + ")");
    }
    if (m_inner != nullptr)
    {
        m_inner->draw_indexed(index_count, instance_count, first_index, base_vertex, first_instance);
    }
}

void RHIValidationContext::push_debug_group(const char* name)
{
    rhi_require_non_null(name, "push_debug_group");
    if (m_inner != nullptr)
    {
        m_inner->push_debug_group(name);
    }
    if (m_debug_depth < MAX_BREADCRUMBS)
    {
        m_breadcrumbs[m_debug_depth] = name;
    }
    ++m_debug_depth;
}

void RHIValidationContext::pop_debug_group()
{
    const uint32_t floor = m_in_pass ? m_pass_debug_base : 0;
    if (m_debug_depth <= floor)
    {
        fail("pop_debug_group", "pop without a matching push");
    }
    if (m_inner != nullptr)
    {
        m_inner->pop_debug_group();
    }
    --m_debug_depth;
}

void RHIValidationContext::end_pass()
{
    require_pass("end_pass");
    if (m_debug_depth != m_pass_debug_base)
    {
        fail("end_pass", "unbalanced debug group");
    }
    if (m_inner != nullptr)
    {
        m_inner->end_pass();
    }
    m_in_pass = false;
    m_pipeline = nullptr;
    m_has_index_buffer = false;
    m_vertex_slots_bound = 0;
    std::memset(m_bound_elements, 0, sizeof(m_bound_elements));
}

void RHIValidationContext::copy_buffer(RHIBuffer& source, uint32_t source_offset, RHIBuffer& destination, uint32_t destination_offset, uint32_t size)
{
    if (m_in_pass)
    {
        fail("copy_buffer", "a copy is not allowed inside a pass");
    }
    if (&source == &destination)
    {
        fail("copy_buffer", "source and destination are the same buffer");
    }
    if (!has_flag(source.usage(), RHIBufferUsage::CopySource))
    {
        fail("copy_buffer", "source lacks the required usage");
    }
    if (!has_flag(destination.usage(), RHIBufferUsage::CopyDest))
    {
        fail("copy_buffer", "destination lacks the required usage");
    }
    if (size == 0 || source_offset > source.size() || size > source.size() - source_offset || destination_offset > destination.size() || size > destination.size() - destination_offset)
    {
        fail("copy_buffer", "range out of bounds");
    }
    if (m_inner != nullptr)
    {
        m_inner->copy_buffer(source, source_offset, destination, destination_offset, size);
    }
}

} // namespace oryx
