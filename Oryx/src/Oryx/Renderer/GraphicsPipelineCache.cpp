#include "oxpch.h"
#include "Oryx/Renderer/GraphicsPipelineCache.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Core/Fnv.h"

namespace oryx
{

namespace
{

template<typename E>
void mix_enum(Fnv1a& hash, E value)
{
    hash.mix_value(static_cast<uint64_t>(value));
}

void mix_stencil_face(Fnv1a& hash, const RHIStencilFace& face)
{
    mix_enum(hash, face.fail);
    mix_enum(hash, face.depth_fail);
    mix_enum(hash, face.pass);
    mix_enum(hash, face.compare);
}

void mix_blend(Fnv1a& hash, const RHIBlendState& blend)
{
    hash.mix_value(blend.enabled ? 1 : 0);
    mix_enum(hash, blend.src_colour);
    mix_enum(hash, blend.dst_colour);
    mix_enum(hash, blend.colour_op);
    mix_enum(hash, blend.src_alpha);
    mix_enum(hash, blend.dst_alpha);
    mix_enum(hash, blend.alpha_op);
    mix_enum(hash, blend.write_mask);
}

void mix_shader(Fnv1a& hash, const Shader& shader)
{
    hash.mix_value(shader.hash());
    hash.mix_value(shader.permutation());
}

} // namespace

uint64_t hash_graphics_pipeline_desc(const GraphicsPipelineDesc& desc)
{
    if (!desc.shaders.vertex || !desc.shaders.pixel)
    {
        throw Error("a pipeline needs a vertex and a pixel shader");
    }
    const GraphicsPipelineState& state = desc.state;
    Fnv1a hash;
    mix_shader(hash, *desc.shaders.vertex);
    mix_shader(hash, *desc.shaders.pixel);

    hash.mix_value(state.vertex_declaration.hash());
    mix_enum(hash, state.topology);
    mix_enum(hash, state.rasterizer.cull);
    mix_enum(hash, state.rasterizer.front_face);
    mix_enum(hash, state.rasterizer.fill);
    for (const RHIBlendState& blend : state.blend)
    {
        mix_blend(hash, blend);
    }
    const RHIDepthStencilState& depth = state.depth_stencil;
    hash.mix_value(depth.depth_test ? 1 : 0);
    hash.mix_value(depth.depth_write ? 1 : 0);
    mix_enum(hash, depth.depth_compare);
    hash.mix_value(depth.stencil_test ? 1 : 0);
    mix_stencil_face(hash, depth.front);
    mix_stencil_face(hash, depth.back);
    hash.mix_value(depth.stencil_read_mask);
    hash.mix_value(depth.stencil_write_mask);
    for (const RHIFormat format : state.colour_formats)
    {
        mix_enum(hash, format);
    }
    hash.mix_value(state.colour_format_count);
    mix_enum(hash, state.depth_format);
    hash.mix_value(state.sample_count);
    return hash.value();
}

GraphicsPipelineHandle GraphicsPipelineCache::get_or_create(IRHI& rhi, const GraphicsPipelineDesc& desc)
{
    const uint64_t key = hash_graphics_pipeline_desc(desc);
    const std::vector<KeyEntry>::iterator position = std::lower_bound(m_keys.begin(), m_keys.end(), key, [](const KeyEntry& entry, uint64_t value) { return entry.key < value; });
    if (position != m_keys.end() && position->key == key)
    {
        ++m_hits;
        return { position->slot, m_generation };
    }

    UniquePtr<GraphicsPipeline> pipeline = create_unique<GraphicsPipeline>(make_graphics_pipeline(rhi, desc));
    const uint32_t slot = static_cast<uint32_t>(m_pipelines.size());
    m_pipelines.push_back(std::move(pipeline));
    m_keys.insert(position, KeyEntry{ key, slot });
    ++m_misses;
    return { slot, m_generation };
}

const GraphicsPipeline& GraphicsPipelineCache::resolve(GraphicsPipelineHandle handle) const
{
    if (!graphics_pipeline_handle_valid(handle))
    {
        throw Error("invalid pipeline handle");
    }
    if (handle.generation != m_generation || handle.index >= m_pipelines.size())
    {
        throw Error("stale pipeline handle", "the pipeline cache was cleared after it was issued; acquire the pipeline again");
    }
    return *m_pipelines[handle.index];
}

void GraphicsPipelineCache::clear()
{
    m_keys.clear();
    m_pipelines.clear();
    ++m_generation;
}

GraphicsPipelineCacheStats GraphicsPipelineCache::stats() const
{
    return { m_hits, m_misses, static_cast<uint32_t>(m_pipelines.size()) };
}

} // namespace oryx
