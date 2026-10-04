#include "oxpch.h"
#include "Oryx/Shaders/ShaderBindings.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

RHIShaderStage to_rhi_stage(ShaderStage stage)
{
    static_assert(SHADER_STAGE_COUNT == RHI_SHADER_STAGE_COUNT, "ShaderStage and RHIShaderStage must list the same stages");
    switch (stage)
    {
    case ShaderStage::Vertex: return RHIShaderStage::Vertex;
    case ShaderStage::Pixel: return RHIShaderStage::Pixel;
    case ShaderStage::Compute: return RHIShaderStage::Compute;
    case ShaderStage::TessControl: return RHIShaderStage::TessControl;
    case ShaderStage::TessEval: return RHIShaderStage::TessEval;
    }
    return RHIShaderStage::Vertex;
}

RHIShaderStageMask to_rhi_stage_mask(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::Vertex: return RHIShaderStageMask::Vertex;
    case ShaderStage::Pixel: return RHIShaderStageMask::Pixel;
    case ShaderStage::Compute:
    case ShaderStage::TessControl:
    case ShaderStage::TessEval: break;
    }
    throw Error(std::string("a ") + shader_stage_name(stage) + " shader cannot be part of a graphics pipeline");
}

RHIBindingKind to_rhi_binding_kind(ShaderBindingKind kind)
{
    switch (kind)
    {
    case ShaderBindingKind::Constants: return RHIBindingKind::Constants;
    case ShaderBindingKind::UniformBuffer: return RHIBindingKind::UniformBuffer;
    case ShaderBindingKind::StorageBuffer: return RHIBindingKind::StorageBuffer;
    case ShaderBindingKind::SampledTexture: return RHIBindingKind::SampledTexture;
    case ShaderBindingKind::StorageTexture: return RHIBindingKind::StorageTexture;
    case ShaderBindingKind::Sampler: return RHIBindingKind::Sampler;
    }
    return RHIBindingKind::Constants;
}

RHIDataType to_rhi_data_type(ShaderTextureData data)
{
    switch (data)
    {
    case ShaderTextureData::Float: return RHIDataType::Float;
    case ShaderTextureData::Int: return RHIDataType::Int;
    case ShaderTextureData::UInt: return RHIDataType::UInt;
    case ShaderTextureData::Depth: return RHIDataType::Depth;
    }
    return RHIDataType::Float;
}

RHITextureDimension to_rhi_texture_dimension(ShaderTextureDimension dimension)
{
    switch (dimension)
    {
    case ShaderTextureDimension::Tex2D: return RHITextureDimension::Tex2D;
    case ShaderTextureDimension::Tex2DArray: return RHITextureDimension::Tex2DArray;
    case ShaderTextureDimension::Cube: return RHITextureDimension::Cube;
    case ShaderTextureDimension::Tex3D: return RHITextureDimension::Tex3D;
    case ShaderTextureDimension::Tex2DMultisample: return RHITextureDimension::Tex2DMultisample;
    }
    return RHITextureDimension::Tex2D;
}

RHIVertexFormat to_rhi_vertex_format(const ShaderDataType& type)
{
    if (type.scalar == ShaderScalar::Float && type.columns == 1)
    {
        switch (type.rows)
        {
        case 1: return RHIVertexFormat::Float;
        case 2: return RHIVertexFormat::Float2;
        case 3: return RHIVertexFormat::Float3;
        case 4: return RHIVertexFormat::Float4;
        default: break;
        }
    }
    throw Error("vertex input type '" + shader_type_name(type) + "' is not supported; use float, float2, float3 or float4");
}

namespace
{

RHIBindingDesc to_rhi_binding(const ShaderBinding& binding, ShaderStage stage)
{
    RHIBindingDesc desc;
    desc.kind = to_rhi_binding_kind(binding.kind);
    desc.stage_mask = to_rhi_stage_mask(stage);
    desc.slot = binding.slot;
    desc.array_count = binding.array_count;
    desc.size = binding.size;
    if (rhi_binding_is_texture(desc.kind))
    {
        desc.data_type = to_rhi_data_type(binding.texture_data_type);
        desc.texture_dimension = to_rhi_texture_dimension(binding.texture_dimension);
    }
    return desc;
}

bool same_binding(const RHIBindingDesc& a, const RHIBindingDesc& b)
{
    return a.kind == b.kind && a.slot == b.slot && a.array_count == b.array_count && a.size == b.size && a.data_type == b.data_type && a.texture_dimension == b.texture_dimension;
}

void merge_stage(const ShaderReflection& reflection, ShaderBindingLayout& layout)
{
    for (const ShaderBinding& binding : reflection.parameters)
    {
        const RHIBindingDesc desc = to_rhi_binding(binding, reflection.stage);
        const std::vector<std::string>::const_iterator found = std::find(layout.names.begin(), layout.names.end(), binding.name);
        if (found == layout.names.end())
        {
            layout.names.push_back(binding.name);
            layout.bindings.push_back(desc);
            continue;
        }
        RHIBindingDesc& existing = layout.bindings[static_cast<size_t>(found - layout.names.begin())];
        if (!same_binding(existing, desc))
        {
            throw Error("binding '" + binding.name + "' differs between the vertex and pixel shader");
        }
        existing.stage_mask |= desc.stage_mask;
    }
}

} // namespace

ShaderBindingLayout to_rhi_binding_layout(const ShaderReflection& vertex, const ShaderReflection& pixel)
{
    if (vertex.stage != ShaderStage::Vertex || pixel.stage != ShaderStage::Pixel)
    {
        throw Error("to_rhi_binding_layout needs a vertex and a pixel reflection");
    }
    ShaderBindingLayout layout;
    merge_stage(vertex, layout);
    merge_stage(pixel, layout);
    if (layout.bindings.size() > RHI_MAX_BINDINGS)
    {
        throw Error("shader set has more than " + std::to_string(RHI_MAX_BINDINGS) + " bindings");
    }
    return layout;
}

} // namespace oryx
