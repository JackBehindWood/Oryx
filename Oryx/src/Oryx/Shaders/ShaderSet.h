#pragma once

#include "Oryx/Graphics/Resources/Pipeline.h"
#include "Oryx/Shaders/PixelShader.h"
#include "Oryx/Shaders/VertexShader.h"

namespace oryx
{

// The stages one graphics draw runs.
struct ShaderSet
{
    Ref<VertexShader> vertex;
    Ref<PixelShader> pixel;
};

// Throws Error unless both stages exist, every pixel input is a vertex output of the same name and type, and the bindings merge.
void validate_shader_set(const ShaderSet& set);

// Validates the set and the vertex layout against the vertex inputs, merges the bindings and creates the RHI pipeline.
[[nodiscard]] Pipeline make_pipeline(IRHI& rhi, const ShaderSet& set, const PipelineState& state);

} // namespace oryx
