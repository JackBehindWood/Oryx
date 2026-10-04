#include "oxpch.h"
#include "MetalPipeline.h"

#include "MetalConvert.h"
#include "Oryx/Core/Error.h"

namespace oryx::metal
{

namespace
{

NS::SharedPtr<MTL::VertexDescriptor> make_vertex_descriptor(const RHIVertexInput& input)
{
    NS::SharedPtr<MTL::VertexDescriptor> descriptor = NS::RetainPtr(MTL::VertexDescriptor::vertexDescriptor());
    for (uint32_t i = 0; i < input.attribute_count; ++i)
    {
        const RHIVertexAttribute& attribute = input.attributes[i];
        MTL::VertexAttributeDescriptor* out = descriptor->attributes()->object(attribute.location);
        out->setFormat(to_mtl(attribute.format));
        out->setOffset(attribute.offset);
        out->setBufferIndex(METAL_VERTEX_STREAM_BASE + attribute.slot);
    }
    for (uint32_t slot = 0; slot < RHI_MAX_VERTEX_SLOTS; ++slot)
    {
        const RHIVertexStream& stream = input.streams[slot];
        if (stream.stride == 0)
        {
            continue;
        }
        MTL::VertexBufferLayoutDescriptor* layout = descriptor->layouts()->object(METAL_VERTEX_STREAM_BASE + slot);
        layout->setStride(stream.stride);
        layout->setStepFunction(stream.step_function == RHIVertexStep::PerInstance ? MTL::VertexStepFunctionPerInstance : MTL::VertexStepFunctionPerVertex);
        layout->setStepRate(stream.step_rate);
    }
    return descriptor;
}

void set_blend(MTL::RenderPipelineColorAttachmentDescriptor& attachment, const RHIBlendState& blend)
{
    attachment.setWriteMask(to_mtl(blend.write_mask));
    attachment.setBlendingEnabled(blend.enabled);
    attachment.setSourceRGBBlendFactor(to_mtl(blend.src_colour));
    attachment.setDestinationRGBBlendFactor(to_mtl(blend.dst_colour));
    attachment.setRgbBlendOperation(to_mtl(blend.colour_op));
    attachment.setSourceAlphaBlendFactor(to_mtl(blend.src_alpha));
    attachment.setDestinationAlphaBlendFactor(to_mtl(blend.dst_alpha));
    attachment.setAlphaBlendOperation(to_mtl(blend.alpha_op));
}

#ifdef OX_DEBUG

MTL::BindingType binding_type(RHIBindingKind kind)
{
    if (rhi_binding_is_texture(kind))
    {
        return MTL::BindingTypeTexture;
    }
    return kind == RHIBindingKind::Sampler ? MTL::BindingTypeSampler : MTL::BindingTypeBuffer;
}

void check_stage_bindings(NS::Array* reflected, const RHIGraphicsPipelineDesc& desc, RHIShaderStageMask stage, const char* stage_name)
{
    for (uint32_t i = 0; i < desc.binding_count; ++i)
    {
        const RHIBindingDesc& layout = desc.bindings[i];
        if (!has_flag(layout.stage_mask, stage))
        {
            continue;
        }
        const MTL::Binding* found = nullptr;
        for (NS::UInteger j = 0; j < reflected->count(); ++j)
        {
            const MTL::Binding* candidate = static_cast<const MTL::Binding*>(reflected->object(j));
            if (candidate->type() == binding_type(layout.kind) && candidate->index() == layout.slot)
            {
                found = candidate;
                break;
            }
        }
        if (found == nullptr)
        {
            throw Error("Metal reflection mismatch: no " + std::string(stage_name) + " binding at the layout's slot and kind", "binding id " + std::to_string(i) + ", slot " + std::to_string(layout.slot));
        }
        if (layout.kind == RHIBindingKind::Constants)
        {
            const MTL::BufferBinding* buffer = static_cast<const MTL::BufferBinding*>(found);
            if (buffer->bufferDataSize() != layout.size)
            {
                throw Error("Metal reflection mismatch: constants size differs from the layout", "binding id " + std::to_string(i) + ", reflected " + std::to_string(buffer->bufferDataSize()) + " bytes, layout " + std::to_string(layout.size));
            }
        }
        else if (rhi_binding_is_texture(layout.kind))
        {
            const MTL::TextureBinding* texture = static_cast<const MTL::TextureBinding*>(found);
            if (texture->arrayLength() != layout.array_count)
            {
                throw Error("Metal reflection mismatch: texture array length differs from the layout", "binding id " + std::to_string(i));
            }
        }
    }
}

void check_vertex_attributes(MTL::Function& vertex, const RHIVertexInput& input)
{
    NS::Array* attributes = vertex.vertexAttributes();
    for (NS::UInteger i = 0; attributes != nullptr && i < attributes->count(); ++i)
    {
        const MTL::VertexAttribute* attribute = static_cast<const MTL::VertexAttribute*>(attributes->object(i));
        if (!attribute->isActive())
        {
            continue;
        }
        bool provided = false;
        for (uint32_t j = 0; j < input.attribute_count; ++j)
        {
            provided = provided || input.attributes[j].location == attribute->attributeIndex();
        }
        if (!provided)
        {
            throw Error("Metal reflection mismatch: the vertex shader reads an attribute the vertex input does not provide", "attribute " + std::to_string(attribute->attributeIndex()));
        }
    }
}

#endif

} // namespace

Ref<MetalPipeline> create_metal_pipeline(const MetalDevice& device, const RHIGraphicsPipelineDesc& desc)
{
    rhi_validate_graphics_pipeline_desc(desc);
    MetalVertexShader* vertex = dynamic_cast<MetalVertexShader*>(desc.vertex.get());
    MetalPixelShader* pixel = dynamic_cast<MetalPixelShader*>(desc.pixel.get());
    if (vertex == nullptr || pixel == nullptr)
    {
        throw Error("RHI pipeline shaders belong to a different backend");
    }
    for (uint32_t i = 0; i < desc.binding_count; ++i)
    {
        if (desc.bindings[i].kind == RHIBindingKind::StorageBuffer || desc.bindings[i].kind == RHIBindingKind::StorageTexture)
        {
            throw Error("RHI storage bindings are not supported by Metal yet");
        }
    }

    NS::SharedPtr<MTL::RenderPipelineDescriptor> descriptor = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
    descriptor->setVertexFunction(vertex->mtl());
    descriptor->setFragmentFunction(pixel->mtl());
    descriptor->setVertexDescriptor(make_vertex_descriptor(desc.vertex_input).get());
    descriptor->setInputPrimitiveTopology(to_mtl_class(desc.topology));
    descriptor->setRasterSampleCount(desc.sample_count);
    for (uint32_t i = 0; i < desc.colour_format_count; ++i)
    {
        MTL::RenderPipelineColorAttachmentDescriptor* attachment = descriptor->colorAttachments()->object(i);
        attachment->setPixelFormat(to_mtl(desc.colour_formats[i]));
        set_blend(*attachment, desc.blend[i]);
    }
    if (desc.depth_format != RHIFormat::Undefined)
    {
        descriptor->setDepthAttachmentPixelFormat(to_mtl(desc.depth_format));
    }

    NS::Error* error = nullptr;
    NS::SharedPtr<MTL::RenderPipelineState> state;
#ifdef OX_DEBUG
    MTL::RenderPipelineReflection* reflection = nullptr;
    state = NS::TransferPtr(device.device()->newRenderPipelineState(descriptor.get(), MTL::PipelineOptionBindingInfo, &reflection, &error));
#else
    state = NS::TransferPtr(device.device()->newRenderPipelineState(descriptor.get(), &error));
#endif
    if (!state)
    {
        throw Error("Metal pipeline creation failed", error != nullptr ? to_string(error->localizedDescription()) : std::string("unknown error"));
    }
#ifdef OX_DEBUG
    if (reflection != nullptr)
    {
        check_stage_bindings(reflection->vertexBindings(), desc, RHIShaderStageMask::Vertex, "vertex");
        check_stage_bindings(reflection->fragmentBindings(), desc, RHIShaderStageMask::Pixel, "pixel");
    }
    check_vertex_attributes(*vertex->mtl(), desc.vertex_input);
#endif

    MetalRasterState raster;
    raster.cull = to_mtl(desc.rasterizer.cull);
    raster.winding = to_mtl(desc.rasterizer.front_face);
    raster.fill = to_mtl(desc.rasterizer.fill);
    raster.primitive = to_mtl(desc.topology);
    return make_ref<MetalPipeline>(desc, std::move(state), device.make_depth_stencil_state(desc.depth_stencil), raster);
}

} // namespace oryx::metal
