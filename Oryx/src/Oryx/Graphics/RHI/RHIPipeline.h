#pragma once

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Graphics/RHI/RHIBinding.h"
#include "Oryx/Graphics/RHI/RHIFormat.h"
#include "Oryx/Graphics/RHI/RHIRenderState.h"
#include "Oryx/Graphics/RHI/RHIResource.h"
#include "Oryx/Graphics/RHI/RHIShader.h"

namespace oryx
{

// A backend's reflection of the created pipeline disagrees with what the layout or vertex input claims; `binding` is the layout index,
// or RHI_INVALID_BINDING for a vertex input, so callers holding the shader-side names can say which one.
class RHIInterfaceMismatch : public Error
{
public:
    RHIInterfaceMismatch(const std::string& message, RHIBindingId binding, std::string detail = "")
        : Error(message, std::move(detail))
        , m_binding(binding)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }

private:
    RHIBindingId m_binding;
};

// Shaders, attributes and bindings are only read during creation; the pipeline retains no shaders and copies the binding table.
struct RHIGraphicsPipelineDesc
{
    RHIVertexShaderPtr vertex;
    RHIPixelShaderPtr pixel;
    RHIVertexInput vertex_input;
    RHITopology topology = RHITopology::Triangles;
    RHIRasterizerState rasterizer;
    RHIBlendState blend[RHI_MAX_COLOUR_TARGETS];
    RHIDepthStencilState depth_stencil;
    RHIFormat colour_formats[RHI_MAX_COLOUR_TARGETS] = { RHIFormat::BGRA8Unorm };
    uint32_t colour_format_count = 1;
    RHIFormat depth_format = RHIFormat::Undefined;
    uint32_t sample_count = 1;
    const RHIBindingDesc* bindings = nullptr;
    uint32_t binding_count = 0;
};

// Throws Error on a malformed description; every backend calls it before building a pipeline.
void rhi_validate_graphics_pipeline_desc(const RHIGraphicsPipelineDesc& desc);

// Common base of graphics and (later) compute pipelines; owns the binding table that commands are validated against.
class RHIPipeline : public RHIResource
{
public:
    [[nodiscard]] RHIPipelineKind kind() const { return m_kind; }
    [[nodiscard]] const RHIBindingDesc* bindings() const { return m_bindings; }
    [[nodiscard]] uint32_t binding_count() const { return m_binding_count; }
    // Throws Error when `id` is not an index into the table.
    [[nodiscard]] const RHIBindingDesc& binding(RHIBindingId id) const;

protected:
    RHIPipeline(RHIPipelineKind kind, const RHIBindingDesc* bindings, uint32_t binding_count);

private:
    RHIBindingDesc m_bindings[RHI_MAX_BINDINGS];
    uint32_t m_binding_count;
    RHIPipelineKind m_kind;
};

class RHIGraphicsPipeline : public RHIPipeline
{
public:
    [[nodiscard]] const RHIFormat* colour_formats() const { return m_colour_formats; }
    [[nodiscard]] uint32_t colour_format_count() const { return m_colour_format_count; }
    [[nodiscard]] RHIFormat depth_format() const { return m_depth_format; }
    [[nodiscard]] uint32_t sample_count() const { return m_sample_count; }
    // Bit n is set when a vertex attribute reads from vertex buffer slot n.
    [[nodiscard]] uint32_t vertex_slot_mask() const { return m_vertex_slot_mask; }

protected:
    explicit RHIGraphicsPipeline(const RHIGraphicsPipelineDesc& desc);

private:
    RHIFormat m_colour_formats[RHI_MAX_COLOUR_TARGETS];
    uint32_t m_colour_format_count;
    RHIFormat m_depth_format;
    uint32_t m_sample_count;
    uint32_t m_vertex_slot_mask;
};

using RHIGraphicsPipelinePtr = Ref<RHIGraphicsPipeline>;

} // namespace oryx
