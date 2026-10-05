#pragma once

#include "Oryx/Shaders/GraphicsShaderSet.h"
#include "Oryx/Shaders/ShaderLibrary.h"
#include "Oryx/Shaders/StaticShader.h"

namespace oryx
{

// Marks a shader the engine owns (registered from Shaders/Builtin); StaticShader stays the generic base for application shaders.
template<typename StageShader>
class BuiltinShader : public StaticShader<StageShader>
{
public:
    using StaticShader<StageShader>::StaticShader;
};

// Base of an effect tag: names the vertex and pixel shader types; the derived tag adds `TOPOLOGY` and `BLEND`.
template<typename VertexStage, typename PixelStage>
struct BuiltinEffect
{
    using VS = VertexStage;
    using PS = PixelStage;
};

// The pipeline description of an effect; the declaration comes from the caller so Shaders/ stays below Renderer/. `permutation` picks the pixel shader's variant.
template<typename Effect>
[[nodiscard]] GraphicsPipelineDesc builtin_desc(const ShaderLibrary& shaders, const RHIVertexDeclaration& declaration, RHIFormat colour_format, uint32_t permutation = 0)
{
    GraphicsPipelineDesc desc;
    desc.shaders = { shaders.get<typename Effect::VS>(), shaders.get<typename Effect::PS>(permutation) };
    desc.state.vertex_declaration = declaration;
    desc.state.topology = Effect::TOPOLOGY;
    desc.state.colour_formats[0] = colour_format;
    if constexpr (Effect::BLEND)
    {
        desc.state.blend[0] = rhi_blend_alpha();
    }
    return desc;
}

} // namespace oryx
