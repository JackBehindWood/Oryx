#pragma once

#include "Oryx/Graphics/RHI/RHIBinding.h"
#include "Oryx/Graphics/RHI/RHIRenderState.h"
#include "Oryx/Graphics/RHI/RHIShader.h"
#include "Oryx/Shaders/ShaderReflection.h"

namespace oryx
{

// The one place Shaders types are translated to RHI types.
[[nodiscard]] RHIShaderStage to_rhi_stage(ShaderStage stage);
// Throws Error for stages that are not part of a graphics pipeline.
[[nodiscard]] RHIShaderStageMask to_rhi_stage_mask(ShaderStage stage);
[[nodiscard]] RHIBindingKind to_rhi_binding_kind(ShaderBindingKind kind);
[[nodiscard]] RHIDataType to_rhi_data_type(ShaderTextureData data);
[[nodiscard]] RHITextureDimension to_rhi_texture_dimension(ShaderTextureDimension dimension);
// Throws Error for anything but float scalars and float2-4.
[[nodiscard]] RHIVertexFormat to_rhi_vertex_format(const ShaderDataType& type);

// `names[i]` is the shader-side name of `bindings[i]`.
struct ShaderBindingLayout
{
    std::vector<RHIBindingDesc> bindings;
    std::vector<std::string> names;
};

// Merges both stages' parameters: a name must keep one slot, kind and shape across stages, and its stage masks union.
[[nodiscard]] ShaderBindingLayout to_rhi_binding_layout(const ShaderReflection& vertex, const ShaderReflection& pixel);

} // namespace oryx
