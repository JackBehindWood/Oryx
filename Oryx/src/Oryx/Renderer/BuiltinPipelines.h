#pragma once

#include "Oryx/Renderer/GraphicsPipelineHandle.h"
#include "Oryx/Shaders/ShaderLibrary.h"
#include "Oryx/Shaders/GraphicsShaderSet.h"

namespace oryx
{

// Descriptions of the engine's own pipelines for a colour target of `colour_format`; acquire one with Renderer::pipeline.
[[nodiscard]] GraphicsPipelineDesc solid_triangles_desc(const ShaderLibrary& shaders, RHIFormat colour_format);
[[nodiscard]] GraphicsPipelineDesc solid_lines_desc(const ShaderLibrary& shaders, RHIFormat colour_format);
// `permutation` picks the pixel shader's texture-array size (see QuadPS).
[[nodiscard]] GraphicsPipelineDesc quad_desc(const ShaderLibrary& shaders, RHIFormat colour_format, uint32_t permutation = 0);
[[nodiscard]] GraphicsPipelineDesc circle_desc(const ShaderLibrary& shaders, RHIFormat colour_format);

// The same pipelines for the Renderer's shaders and back-buffer format, acquired through the Renderer; handles stay valid until Renderer::release_pipelines.
[[nodiscard]] GraphicsPipelineHandle builtin_solid_triangles();
[[nodiscard]] GraphicsPipelineHandle builtin_solid_lines();
[[nodiscard]] GraphicsPipelineHandle builtin_quad(uint32_t permutation = 0);
[[nodiscard]] GraphicsPipelineHandle builtin_circle();

} // namespace oryx
