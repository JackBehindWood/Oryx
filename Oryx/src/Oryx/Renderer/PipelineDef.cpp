#include "oxpch.h"
#include "Oryx/Renderer/PipelineDef.h"

namespace oryx
{

bool operator==(const PipelineDef& a, const PipelineDef& b)
{
    return a.shaders == b.shaders && a.declaration == b.declaration && a.topology == b.topology && a.blend == b.blend && a.depth_test == b.depth_test && a.depth_write == b.depth_write;
}

GraphicsPipelineDesc pipeline_desc(const PipelineDef& def, const ShaderLibrary& shaders, const PassFormats& formats, uint32_t permutation)
{
    GraphicsPipelineDesc desc;
    desc.shaders = def.shaders(shaders, permutation);
    desc.state.vertex_declaration = *def.declaration;
    desc.state.topology = def.topology;
    desc.state.colour_formats[0] = formats.colour;
    desc.state.depth_format = formats.depth;
    desc.state.depth_stencil.depth_test = def.depth_test;
    desc.state.depth_stencil.depth_write = def.depth_write;
    if (def.blend)
    {
        desc.state.blend[0] = rhi_blend_alpha();
    }
    return desc;
}

GraphicsPipelineHandle PipelineMemo::get(IRHI& rhi, GraphicsPipelineCache& cache, const ShaderLibrary& shaders, const PassFormats& formats, const PipelineDef& def, uint32_t permutation)
{
    if (m_generation != cache.generation())
    {
        reset();
        m_generation = cache.generation();
    }
    for (const Entry& entry : m_entries)
    {
        if (entry.permutation == permutation && entry.formats == formats && entry.def == def)
        {
            return entry.handle;
        }
    }
    const GraphicsPipelineHandle handle = cache.get_or_create(rhi, pipeline_desc(def, shaders, formats, permutation));
    m_entries.push_back({ def, formats, permutation, handle });
    return handle;
}

void PipelineMemo::reset()
{
    m_entries.clear();
}

} // namespace oryx
