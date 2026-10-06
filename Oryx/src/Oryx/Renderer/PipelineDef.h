#pragma once

#include "Oryx/Renderer/GraphicsPipelineCache.h"
#include "Oryx/Shaders/ShaderLibrary.h"

namespace oryx
{

class IRHI;

// Everything a pass fixes about a pipeline except the colour format and the pixel shader's permutation (Unreal's FGraphicsPipelineStateInitializer);
// build it with make_pipeline_def, lazily (shader types register during static initialisation).
struct PipelineDef
{
    GraphicsShaderSet (*shaders)(const ShaderLibrary&, uint32_t permutation) = nullptr;
    const RHIVertexDeclaration* declaration = nullptr;
    RHITopology topology = RHITopology::Triangles;
    bool blend = false;
};

[[nodiscard]] bool operator==(const PipelineDef& a, const PipelineDef& b);

// `Vertex` supplies the declaration through vertex_declaration<Vertex>(); the vertex shader has no permutations, `permutation` picks the pixel shader's.
template<typename VertexStage, typename PixelStage, typename Vertex>
[[nodiscard]] PipelineDef make_pipeline_def(RHITopology topology, bool blend)
{
    return { [](const ShaderLibrary& library, uint32_t permutation) { return GraphicsShaderSet{ library.get<VertexStage>(), library.get<PixelStage>(permutation) }; }, &vertex_declaration<Vertex>(), topology, blend };
}

// Throws Error when the pixel shader has no such permutation.
[[nodiscard]] GraphicsPipelineDesc pipeline_desc(const PipelineDef& def, const ShaderLibrary& shaders, RHIFormat colour_format, uint32_t permutation = 0);

// Remembers the handles of the defs a renderer draws with until the cache is cleared or the colour format changes.
class PipelineMemo
{
public:
    [[nodiscard]] GraphicsPipelineHandle get(IRHI& rhi, GraphicsPipelineCache& cache, const ShaderLibrary& shaders, RHIFormat colour_format, const PipelineDef& def, uint32_t permutation = 0);
    void reset();

private:
    struct Entry
    {
        PipelineDef def;
        uint32_t permutation;
        GraphicsPipelineHandle handle;
    };

    std::vector<Entry> m_entries;
    uint32_t m_generation = 0;
    RHIFormat m_format = RHIFormat::Undefined;
};

} // namespace oryx
