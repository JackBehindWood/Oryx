#pragma once

#include "Oryx/Renderer/GraphicsPipelineCache.h"
#include "Oryx/Shaders/ShaderLibrary.h"

namespace oryx
{

class IRHI;

// The attachment formats of the pass a pipeline draws into; a depth format other than Undefined means the pass has a depth attachment.
struct PassFormats
{
    RHIFormat colour = RHIFormat::BGRA8Unorm;
    RHIFormat depth = RHIFormat::Undefined;
};

[[nodiscard]] constexpr bool operator==(const PassFormats& a, const PassFormats& b)
{
    return a.colour == b.colour && a.depth == b.depth;
}

// Everything a pass fixes about a pipeline except the colour format and the pixel shader's permutation (Unreal's FGraphicsPipelineStateInitializer);
// build it with make_pipeline_def, lazily (shader types register during static initialisation).
struct PipelineDef
{
    GraphicsShaderSet (*shaders)(const ShaderLibrary&, uint32_t permutation) = nullptr;
    const RHIVertexDeclaration* declaration = nullptr;
    RHITopology topology = RHITopology::Triangles;
    bool blend = false;
    bool depth_test = false;
    bool depth_write = false;
};

[[nodiscard]] bool operator==(const PipelineDef& a, const PipelineDef& b);

// `Vertex` supplies the declaration through vertex_declaration<Vertex>(); the vertex shader has no permutations, `permutation` picks the pixel shader's.
template<typename VertexStage, typename PixelStage, typename Vertex>
[[nodiscard]] PipelineDef make_pipeline_def(RHITopology topology, bool blend, bool depth_test = false, bool depth_write = false)
{
    return { [](const ShaderLibrary& library, uint32_t permutation) { return GraphicsShaderSet{ library.get<VertexStage>(), library.get<PixelStage>(permutation) }; }, &vertex_declaration<Vertex>(), topology, blend, depth_test, depth_write };
}

// Throws Error when the pixel shader has no such permutation.
[[nodiscard]] GraphicsPipelineDesc pipeline_desc(const PipelineDef& def, const ShaderLibrary& shaders, const PassFormats& formats, uint32_t permutation = 0);
[[nodiscard]] inline GraphicsPipelineDesc pipeline_desc(const PipelineDef& def, const ShaderLibrary& shaders, RHIFormat colour_format, uint32_t permutation = 0)
{
    return pipeline_desc(def, shaders, PassFormats{ colour_format, RHIFormat::Undefined }, permutation);
}

// Remembers the handles of the defs a renderer draws with, per pass format, until the cache is cleared.
class PipelineMemo
{
public:
    [[nodiscard]] GraphicsPipelineHandle get(IRHI& rhi, GraphicsPipelineCache& cache, const ShaderLibrary& shaders, const PassFormats& formats, const PipelineDef& def, uint32_t permutation = 0);
    [[nodiscard]] GraphicsPipelineHandle get(IRHI& rhi, GraphicsPipelineCache& cache, const ShaderLibrary& shaders, RHIFormat colour_format, const PipelineDef& def, uint32_t permutation = 0)
    {
        return get(rhi, cache, shaders, PassFormats{ colour_format, RHIFormat::Undefined }, def, permutation);
    }
    void reset();

private:
    struct Entry
    {
        PipelineDef def;
        PassFormats formats;
        uint32_t permutation;
        GraphicsPipelineHandle handle;
    };

    std::vector<Entry> m_entries;
    uint32_t m_generation = 0;
};

} // namespace oryx
