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

#ifndef OX_DIST

MTL::BindingType binding_type(RHIBindingKind kind)
{
    if (rhi_binding_is_texture(kind))
    {
        return MTL::BindingTypeTexture;
    }
    return kind == RHIBindingKind::Sampler ? MTL::BindingTypeSampler : MTL::BindingTypeBuffer;
}

MTL::TextureType texture_type(RHITextureDimension dimension)
{
    switch (dimension)
    {
    case RHITextureDimension::Tex2D: return MTL::TextureType2D;
    case RHITextureDimension::Tex2DArray: return MTL::TextureType2DArray;
    case RHITextureDimension::Cube: return MTL::TextureTypeCube;
    case RHITextureDimension::Tex3D: return MTL::TextureType3D;
    case RHITextureDimension::Tex2DMultisample: return MTL::TextureType2DMultisample;
    }
    return MTL::TextureType2D;
}

// Depth reads as float; only the sampled scalar class is checked.
MTL::DataType texture_data_type(RHIDataType type)
{
    switch (type)
    {
    case RHIDataType::Int: return MTL::DataTypeInt;
    case RHIDataType::UInt: return MTL::DataTypeUInt;
    default: return MTL::DataTypeFloat;
    }
}

// The type a vertex function declares for an attribute a format feeds; half and normalised formats are read as float.
MTL::DataType shader_input_type(RHIVertexFormat format)
{
    switch (format)
    {
    case RHIVertexFormat::Float: return MTL::DataTypeFloat;
    case RHIVertexFormat::Float2:
    case RHIVertexFormat::Half2: return MTL::DataTypeFloat2;
    case RHIVertexFormat::Float3: return MTL::DataTypeFloat3;
    case RHIVertexFormat::Float4:
    case RHIVertexFormat::Half4:
    case RHIVertexFormat::UByte4Norm: return MTL::DataTypeFloat4;
    case RHIVertexFormat::UInt: return MTL::DataTypeUInt;
    case RHIVertexFormat::UInt2: return MTL::DataTypeUInt2;
    case RHIVertexFormat::UInt3: return MTL::DataTypeUInt3;
    case RHIVertexFormat::UInt4: return MTL::DataTypeUInt4;
    case RHIVertexFormat::Int: return MTL::DataTypeInt;
    case RHIVertexFormat::Int2: return MTL::DataTypeInt2;
    case RHIVertexFormat::Int3: return MTL::DataTypeInt3;
    case RHIVertexFormat::Int4: return MTL::DataTypeInt4;
    }
    return MTL::DataTypeFloat;
}

std::string at_binding(const RHIBindingDesc& layout, RHIBindingId id, const char* stage_name)
{
    return std::string(stage_name) + " binding " + std::to_string(id) + " (slot " + std::to_string(layout.slot) + ")";
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
        for (NS::UInteger j = 0; reflected != nullptr && j < reflected->count(); ++j)
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
            throw RHIInterfaceMismatch("Metal reflection mismatch: no " + at_binding(layout, i, stage_name) + " of the layout's kind", i);
        }
        if (layout.kind == RHIBindingKind::Constants)
        {
            const MTL::BufferBinding* buffer = static_cast<const MTL::BufferBinding*>(found);
            if (buffer->bufferDataSize() != layout.size)
            {
                throw RHIInterfaceMismatch("Metal reflection mismatch: " + at_binding(layout, i, stage_name) + " is " + std::to_string(buffer->bufferDataSize()) + " bytes in the shader, " + std::to_string(layout.size) + " in the layout", i);
            }
        }
        else if (rhi_binding_is_texture(layout.kind))
        {
            const MTL::TextureBinding* texture = static_cast<const MTL::TextureBinding*>(found);
            if (texture->arrayLength() != layout.array_count)
            {
                throw RHIInterfaceMismatch("Metal reflection mismatch: " + at_binding(layout, i, stage_name) + " is an array of " + std::to_string(texture->arrayLength()) + " in the shader, " + std::to_string(layout.array_count) + " in the layout", i);
            }
            if (texture->textureType() != texture_type(layout.texture_dimension))
            {
                throw RHIInterfaceMismatch("Metal reflection mismatch: " + at_binding(layout, i, stage_name) + " has a different texture dimension in the shader than in the layout", i);
            }
            if (layout.data_type != RHIDataType::Depth && texture->textureDataType() != texture_data_type(layout.data_type))
            {
                throw RHIInterfaceMismatch("Metal reflection mismatch: " + at_binding(layout, i, stage_name) + " samples a different scalar type in the shader than in the layout", i);
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
        const std::string where = "vertex attribute " + std::to_string(attribute->attributeIndex());
        const RHIVertexAttribute* provided = nullptr;
        for (uint32_t j = 0; j < input.attribute_count; ++j)
        {
            if (input.attributes[j].location == attribute->attributeIndex())
            {
                provided = &input.attributes[j];
            }
        }
        if (provided == nullptr)
        {
            throw RHIInterfaceMismatch("Metal reflection mismatch: the vertex shader reads " + where + ", which the vertex input does not provide", RHI_INVALID_BINDING);
        }
        if (attribute->attributeType() != shader_input_type(provided->format))
        {
            throw RHIInterfaceMismatch("Metal reflection mismatch: " + where + " has a different type in the shader than the vertex input provides", RHI_INVALID_BINDING);
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
#ifndef OX_DIST
    MTL::RenderPipelineReflection* reflection = nullptr;
    state = NS::TransferPtr(device.device()->newRenderPipelineState(descriptor.get(), MTL::PipelineOptionBindingInfo, &reflection, &error));
#else
    state = NS::TransferPtr(device.device()->newRenderPipelineState(descriptor.get(), &error));
#endif
    if (!state)
    {
        throw Error("Metal pipeline creation failed", error != nullptr ? to_string(error->localizedDescription()) : std::string("unknown error"));
    }
#ifndef OX_DIST
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
