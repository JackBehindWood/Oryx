#pragma once

#include "Oryx/Graphics/Resources/GraphicsPipeline.h"
#include "Oryx/Shaders/PixelShader.h"
#include "Oryx/Shaders/VertexShader.h"

namespace oryx
{

// The stages one graphics draw runs.
struct GraphicsShaderSet
{
    VertexShaderPtr vertex;
    PixelShaderPtr pixel;
};

// Everything that identifies one graphics pipeline.
struct GraphicsPipelineDesc
{
    GraphicsShaderSet shaders;
    GraphicsPipelineState state;
};

// Throws Error unless both stages exist, every pixel input is a vertex output of the same name and type, and the bindings merge.
void validate_graphics_shader_set(const GraphicsShaderSet& set);

// Validates the set and the vertex layout against the vertex inputs, merges the bindings and creates the RHI pipeline.
[[nodiscard]] GraphicsPipeline make_graphics_pipeline(IRHI& rhi, const GraphicsShaderSet& set, const GraphicsPipelineState& state);
[[nodiscard]] inline GraphicsPipeline make_graphics_pipeline(IRHI& rhi, const GraphicsPipelineDesc& desc)
{
    return make_graphics_pipeline(rhi, desc.shaders, desc.state);
}

} // namespace oryx
