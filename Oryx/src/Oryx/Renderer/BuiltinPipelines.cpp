#include "oxpch.h"
#include "Oryx/Renderer/BuiltinPipelines.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Renderer/Vertex2D.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

namespace oryx
{

GraphicsPipelineDesc builtin_pipeline_desc(BuiltinPipeline pipeline, const ShaderLibrary& shaders, RHIFormat colour_format, uint32_t permutation)
{
    if (permutation != 0 && pipeline != BuiltinPipeline::Quad && pipeline != BuiltinPipeline::Text)
    {
        throw Error("BuiltinPipeline has no such permutation", std::to_string(permutation));
    }
    switch (pipeline)
    {
    case BuiltinPipeline::SolidTriangles: return builtin_desc<SolidTrianglesEffect>(shaders, vertex_declaration<Vertex2DLine>(), colour_format);
    case BuiltinPipeline::SolidLines: return builtin_desc<LineEffect>(shaders, vertex_declaration<Vertex2DLine>(), colour_format);
    case BuiltinPipeline::Quad: return builtin_desc<QuadEffect>(shaders, vertex_declaration<Vertex2DQuad>(), colour_format, permutation);
    case BuiltinPipeline::Circle: return builtin_desc<CircleEffect>(shaders, vertex_declaration<Vertex2DCircle>(), colour_format);
    case BuiltinPipeline::Text: return builtin_desc<TextEffect>(shaders, vertex_declaration<Vertex2DText>(), colour_format, permutation);
    }
    throw Error("BuiltinPipeline is invalid");
}

GraphicsPipelineHandle BuiltinPipelines::get(IRHI& rhi, GraphicsPipelineCache& cache, const ShaderLibrary& shaders, RHIFormat colour_format, BuiltinPipeline pipeline, uint32_t permutation)
{
    if (static_cast<uint32_t>(pipeline) >= BUILTIN_PIPELINE_COUNT || permutation >= BUILTIN_MAX_PERMUTATIONS)
    {
        throw Error("BuiltinPipelines::get received an unknown pipeline or permutation");
    }
    if (m_generation != cache.generation() || m_format != colour_format)
    {
        reset();
        m_generation = cache.generation();
        m_format = colour_format;
    }
    GraphicsPipelineHandle& handle = m_handles[static_cast<uint32_t>(pipeline)][permutation];
    if (!graphics_pipeline_handle_valid(handle))
    {
        handle = cache.get_or_create(rhi, builtin_pipeline_desc(pipeline, shaders, colour_format, permutation));
    }
    return handle;
}

void BuiltinPipelines::reset()
{
    for (GraphicsPipelineHandle (&row)[BUILTIN_MAX_PERMUTATIONS] : m_handles)
    {
        for (GraphicsPipelineHandle& handle : row)
        {
            handle = {};
        }
    }
}

} // namespace oryx
