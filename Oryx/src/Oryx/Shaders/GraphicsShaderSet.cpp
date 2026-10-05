#include "oxpch.h"
#include "Oryx/Shaders/GraphicsShaderSet.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Shaders/ShaderBindings.h"

namespace oryx
{

namespace
{

const ShaderStageVariable* find_variable(const std::vector<ShaderStageVariable>& variables, const std::string& name)
{
    for (const ShaderStageVariable& variable : variables)
    {
        if (!variable.builtin && variable.name == name)
        {
            return &variable;
        }
    }
    return nullptr;
}

} // namespace

void validate_graphics_shader_set(const GraphicsShaderSet& set)
{
    if (!set.vertex || !set.pixel)
    {
        throw Error("a shader set needs a vertex and a pixel shader");
    }
    const ShaderReflection& vertex = set.vertex->reflection();
    const ShaderReflection& pixel = set.pixel->reflection();
    for (const ShaderStageVariable& input : pixel.inputs)
    {
        if (input.builtin)
        {
            continue;
        }
        const ShaderStageVariable* output = find_variable(vertex.outputs, input.name);
        if (output == nullptr)
        {
            throw Error("pixel input '" + input.name + "' has no matching output in the vertex shader");
        }
        if (!(output->type == input.type))
        {
            throw Error("pixel input '" + input.name + "' is " + shader_type_name(input.type) + " but the vertex shader outputs " + shader_type_name(output->type));
        }
    }
    (void)to_rhi_binding_layout(vertex, pixel);
}

GraphicsPipeline make_graphics_pipeline(IRHI& rhi, const GraphicsShaderSet& set, const GraphicsPipelineState& state)
{
    validate_graphics_shader_set(set);
    for (const ShaderStageVariable& input : set.vertex->reflection().inputs)
    {
        const RHIVertexAttribute* attribute = nullptr;
        for (const RHIVertexAttribute& candidate : state.vertex_declaration.attributes())
        {
            if (candidate.location == input.location)
            {
                attribute = &candidate;
            }
        }
        if (attribute == nullptr)
        {
            throw Error("vertex declaration has no attribute for shader input '" + input.name + "' at location " + std::to_string(input.location));
        }
        if (!rhi_vertex_format_feeds(attribute->format, input.type))
        {
            throw Error("vertex declaration attribute " + std::to_string(input.location) + " does not match shader input '" + input.name + "' (" + shader_type_name(input.type) + ")");
        }
    }

    const ShaderBindingLayout layout = to_rhi_binding_layout(set.vertex->reflection(), set.pixel->reflection());
    RHIGraphicsPipelineDesc desc;
    desc.vertex = set.vertex->rhi();
    desc.pixel = set.pixel->rhi();
    desc.vertex_input = state.vertex_declaration.input();
    desc.topology = state.topology;
    desc.rasterizer = state.rasterizer;
    desc.depth_stencil = state.depth_stencil;
    for (uint32_t i = 0; i < RHI_MAX_COLOUR_TARGETS; ++i)
    {
        desc.blend[i] = state.blend[i];
        desc.colour_formats[i] = state.colour_formats[i];
    }
    desc.colour_format_count = state.colour_format_count;
    desc.depth_format = state.depth_format;
    desc.sample_count = state.sample_count;
    desc.bindings = layout.bindings.data();
    desc.binding_count = static_cast<uint32_t>(layout.bindings.size());
    return GraphicsPipeline(rhi.create_graphics_pipeline(desc), layout.names);
}

} // namespace oryx
