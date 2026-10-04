#include "oxpch.h"
#include "Oryx/Graphics/RHI/RHIPipeline.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

enum class BindingSpace : uint8_t
{
    Buffer,
    Texture,
    Sampler
};

BindingSpace binding_space(RHIBindingKind kind)
{
    if (rhi_binding_is_texture(kind))
    {
        return BindingSpace::Texture;
    }
    return kind == RHIBindingKind::Sampler ? BindingSpace::Sampler : BindingSpace::Buffer;
}

bool overlaps(const RHIBindingDesc& a, const RHIBindingDesc& b)
{
    return binding_space(a.kind) == binding_space(b.kind)
        && (static_cast<uint8_t>(a.stage_mask) & static_cast<uint8_t>(b.stage_mask)) != 0
        && a.slot < b.slot + b.array_count
        && b.slot < a.slot + a.array_count;
}

void validate_binding(const RHIBindingDesc& binding)
{
    if (static_cast<uint8_t>(binding.stage_mask) == 0)
    {
        throw Error("RHI binding has no shader stage");
    }
    if (binding.array_count == 0)
    {
        throw Error("RHI binding array count must be non-zero");
    }
    if (binding.kind == RHIBindingKind::Constants && (binding.size == 0 || binding.size > RHI_MAX_CONSTANTS_SIZE))
    {
        throw Error("RHI constants binding size must be between 1 and 4096 bytes");
    }
    if ((binding.kind == RHIBindingKind::Constants || rhi_binding_is_buffer(binding.kind)) && binding.array_count != 1)
    {
        throw Error("RHI constants and buffer bindings cannot be arrays");
    }
}

void validate_vertex_input(const RHIVertexInput& input)
{
    if (input.attribute_count > 0 && input.attributes == nullptr)
    {
        throw Error("RHI vertex input has a null attribute array");
    }
    for (uint32_t i = 0; i < input.attribute_count; ++i)
    {
        const RHIVertexAttribute& attribute = input.attributes[i];
        if (attribute.slot >= RHI_MAX_VERTEX_SLOTS)
        {
            throw Error("RHI vertex attribute slot is out of range");
        }
        const RHIVertexStream& stream = input.streams[attribute.slot];
        if (stream.stride == 0 || attribute.offset + rhi_vertex_format_bytes(attribute.format) > stream.stride)
        {
            throw Error("RHI vertex attribute does not fit its stream stride");
        }
        for (uint32_t j = 0; j < i; ++j)
        {
            if (input.attributes[j].location == attribute.location)
            {
                throw Error("RHI vertex attribute locations must be unique");
            }
        }
    }
    for (uint32_t slot = 0; slot < RHI_MAX_VERTEX_SLOTS; ++slot)
    {
        if (input.streams[slot].step_rate == 0)
        {
            throw Error("RHI vertex stream step rate must be non-zero");
        }
    }
}

} // namespace

void rhi_validate_graphics_pipeline_desc(const RHIGraphicsPipelineDesc& desc)
{
    if (!desc.vertex || !desc.pixel)
    {
        throw Error("RHI pipeline requires a vertex and a pixel shader");
    }
    if (desc.colour_format_count > RHI_MAX_COLOUR_TARGETS)
    {
        throw Error("RHI pipeline has too many colour targets");
    }
    if (desc.colour_format_count == 0 && desc.depth_format == RHIFormat::Undefined)
    {
        throw Error("RHI pipeline needs a colour or a depth format");
    }
    for (uint32_t i = 0; i < desc.colour_format_count; ++i)
    {
        if (desc.colour_formats[i] == RHIFormat::Undefined)
        {
            throw Error("RHI pipeline colour format is undefined");
        }
    }
    if (desc.depth_format != RHIFormat::Undefined && !rhi_format_is_depth(desc.depth_format))
    {
        throw Error("RHI pipeline depth format is not a depth format");
    }
    if (desc.sample_count == 0 || desc.sample_count > 8 || (desc.sample_count & (desc.sample_count - 1)) != 0)
    {
        throw Error("RHI pipeline sample count must be 1, 2, 4 or 8");
    }
    if (desc.binding_count > RHI_MAX_BINDINGS)
    {
        throw Error("RHI pipeline has too many bindings");
    }
    if (desc.binding_count > 0 && desc.bindings == nullptr)
    {
        throw Error("RHI pipeline has a null binding table");
    }
    for (uint32_t i = 0; i < desc.binding_count; ++i)
    {
        validate_binding(desc.bindings[i]);
        for (uint32_t j = 0; j < i; ++j)
        {
            if (overlaps(desc.bindings[i], desc.bindings[j]))
            {
                throw Error("RHI pipeline bindings overlap in slot and stage");
            }
        }
    }
    validate_vertex_input(desc.vertex_input);
}

RHIPipeline::RHIPipeline(RHIPipelineKind kind, const RHIBindingDesc* bindings, uint32_t binding_count)
    : m_binding_count(binding_count)
    , m_kind(kind)
{
    for (uint32_t i = 0; i < binding_count; ++i)
    {
        m_bindings[i] = bindings[i];
    }
}

const RHIBindingDesc& RHIPipeline::binding(RHIBindingId id) const
{
    if (id >= m_binding_count)
    {
        throw Error("RHI binding id is not in the pipeline's binding table");
    }
    return m_bindings[id];
}

RHIGraphicsPipeline::RHIGraphicsPipeline(const RHIGraphicsPipelineDesc& desc)
    : RHIPipeline(RHIPipelineKind::Graphics, desc.bindings, desc.binding_count)
    , m_colour_format_count(desc.colour_format_count)
    , m_depth_format(desc.depth_format)
    , m_sample_count(desc.sample_count)
{
    for (uint32_t i = 0; i < RHI_MAX_COLOUR_TARGETS; ++i)
    {
        m_colour_formats[i] = desc.colour_formats[i];
    }
}

} // namespace oryx
