#include "oxpch.h"
#include "Oryx/Renderer/BuiltinPipelines.h"

#include "Oryx/Renderer/Renderer.h"
#include "Oryx/Shaders/Static/StaticShaders.h"

namespace oryx
{

namespace
{

GraphicsPipelineDesc make_desc(GraphicsShaderSet shaders, VertexLayout layout, RHITopology topology, bool blend, RHIFormat colour_format)
{
    GraphicsPipelineDesc desc;
    desc.shaders = std::move(shaders);
    desc.state.vertex_layout = std::move(layout);
    desc.state.topology = topology;
    desc.state.colour_formats[0] = colour_format;
    if (blend)
    {
        desc.state.blend[0] = rhi_blend_alpha();
    }
    return desc;
}

} // namespace

GraphicsPipelineDesc solid_triangles_desc(const ShaderLibrary& shaders, RHIFormat colour_format)
{
    return make_desc({ shaders.get<SolidVS>(), shaders.get<SolidPS>() }, solid_vertex_layout(), RHITopology::Triangles, false, colour_format);
}

GraphicsPipelineDesc solid_lines_desc(const ShaderLibrary& shaders, RHIFormat colour_format)
{
    return make_desc({ shaders.get<SolidVS>(), shaders.get<SolidPS>() }, solid_vertex_layout(), RHITopology::Lines, false, colour_format);
}

GraphicsPipelineDesc quad_desc(const ShaderLibrary& shaders, RHIFormat colour_format, uint32_t permutation)
{
    return make_desc({ shaders.get<QuadVS>(), shaders.get<QuadPS>(permutation) }, quad_vertex_layout(), RHITopology::Triangles, true, colour_format);
}

GraphicsPipelineDesc circle_desc(const ShaderLibrary& shaders, RHIFormat colour_format)
{
    return make_desc({ shaders.get<CircleVS>(), shaders.get<CirclePS>() }, circle_vertex_layout(), RHITopology::Triangles, true, colour_format);
}

GraphicsPipelineHandle builtin_solid_triangles()
{
    return Renderer::pipeline(solid_triangles_desc(Renderer::shaders(), Renderer::back_buffer_format()));
}

GraphicsPipelineHandle builtin_solid_lines()
{
    return Renderer::pipeline(solid_lines_desc(Renderer::shaders(), Renderer::back_buffer_format()));
}

GraphicsPipelineHandle builtin_quad(uint32_t permutation)
{
    return Renderer::pipeline(quad_desc(Renderer::shaders(), Renderer::back_buffer_format(), permutation));
}

GraphicsPipelineHandle builtin_circle()
{
    return Renderer::pipeline(circle_desc(Renderer::shaders(), Renderer::back_buffer_format()));
}

} // namespace oryx
