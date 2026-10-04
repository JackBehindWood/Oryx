#pragma once

#include "Oryx/Renderer/GraphicsPipelineHandle.h"
#include "Oryx/Shaders/GraphicsShaderSet.h"

namespace oryx
{

struct GraphicsPipelineCacheStats
{
    uint32_t hits = 0;
    uint32_t misses = 0;
    uint32_t entries = 0;
};

// Deterministic FNV-1a over the shader identities and every GraphicsPipelineState field; extend it with any field added to GraphicsPipelineState.
[[nodiscard]] uint64_t hash_graphics_pipeline_desc(const GraphicsPipelineDesc& desc);

// Owns every pipeline it creates; equal descriptions share one pipeline. Main thread only.
class GraphicsPipelineCache
{
public:
    GraphicsPipelineHandle get_or_create(IRHI& rhi, const GraphicsPipelineDesc& desc);
    // Throws Error for an invalid handle or one issued before the last clear().
    [[nodiscard]] const GraphicsPipeline& resolve(GraphicsPipelineHandle handle) const;
    // Invalidates every handle issued so far.
    void clear();
    [[nodiscard]] uint32_t generation() const { return m_generation; }
    [[nodiscard]] GraphicsPipelineCacheStats stats() const;

private:
    struct KeyEntry
    {
        uint64_t key;
        uint32_t slot;
    };

    std::vector<KeyEntry> m_keys;
    std::vector<UniquePtr<GraphicsPipeline>> m_pipelines;
    uint32_t m_generation = 1;
    uint32_t m_hits = 0;
    uint32_t m_misses = 0;
};

} // namespace oryx
