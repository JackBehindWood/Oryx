#include "oxpch.h"
#include "Oryx/Renderer/PipelineDef.h"

namespace oryx
{

bool operator==(const PipelineDef& a, const PipelineDef& b)
{
    return a.shaders == b.shaders && a.declaration == b.declaration && a.topology == b.topology && a.blend == b.blend;
}

GraphicsPipelineDesc pipeline_desc(const PipelineDef& def, const ShaderLibrary& shaders, RHIFormat colour_format, uint32_t permutation)
{
    GraphicsPipelineDesc desc;
    desc.shaders = def.shaders(shaders, permutation);
    desc.state.vertex_declaration = *def.declaration;
    desc.state.topology = def.topology;
    desc.state.colour_formats[0] = colour_format;
    if (def.blend)
    {
        desc.state.blend[0] = rhi_blend_alpha();
    }
    return desc;
}

GraphicsPipelineHandle PipelineMemo::get(IRHI& rhi, GraphicsPipelineCache& cache, const ShaderLibrary& shaders, RHIFormat colour_format, const PipelineDef& def, uint32_t permutation)
{
    if (m_generation != cache.generation() || m_format != colour_format)
    {
        reset();
        m_generation = cache.generation();
        m_format = colour_format;
    }
    for (const Entry& entry : m_entries)
    {
        if (entry.permutation == permutation && entry.def == def)
        {
            return entry.handle;
        }
    }
    const GraphicsPipelineHandle handle = cache.get_or_create(rhi, pipeline_desc(def, shaders, colour_format, permutation));
    m_entries.push_back({ def, permutation, handle });
    return handle;
}

void PipelineMemo::reset()
{
    m_entries.clear();
}

} // namespace oryx
