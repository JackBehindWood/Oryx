#pragma once

#include "Oryx/Graphics/RHI/RHIPipeline.h"
#include "Oryx/Graphics/Resources/VertexLayout.h"

namespace oryx
{

struct GraphicsPipelineState
{
    VertexLayout vertex_layout;
    RHITopology topology = RHITopology::Triangles;
    RHIRasterizerState rasterizer;
    RHIBlendState blend[RHI_MAX_COLOUR_TARGETS];
    RHIDepthStencilState depth_stencil;
    RHIFormat colour_formats[RHI_MAX_COLOUR_TARGETS] = { RHIFormat::BGRA8Unorm };
    uint32_t colour_format_count = 1;
    RHIFormat depth_format = RHIFormat::Undefined;
    uint32_t sample_count = 1;
};

// Owns an RHI pipeline and resolves binding names to the ids its commands take; `names[i]` names binding id `i`.
class GraphicsPipeline
{
public:
    GraphicsPipeline(RHIGraphicsPipelinePtr pipeline, std::vector<std::string> names);

    [[nodiscard]] RHIGraphicsPipeline& rhi() const { return *m_pipeline; }
    [[nodiscard]] const RHIGraphicsPipelinePtr& rhi_ptr() const { return m_pipeline; }
    // Throws Error for an unknown name.
    [[nodiscard]] RHIBindingId binding(std::string_view name) const;
    // Returns RHI_INVALID_BINDING for an unknown name.
    [[nodiscard]] RHIBindingId try_binding(std::string_view name) const;
    [[nodiscard]] uint32_t binding_count() const { return static_cast<uint32_t>(m_names.size()); }

private:
    RHIGraphicsPipelinePtr m_pipeline;
    std::vector<std::string> m_names;
};

} // namespace oryx
