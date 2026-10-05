#include "oxpch.h"
#include "MetalCommandContext.h"

#include "MetalConvert.h"
#include "Oryx/Core/Error.h"

namespace oryx::metal
{

namespace
{

template<typename T>
T& require_backend(RHIResource& resource, const char* what)
{
    T* converted = dynamic_cast<T*>(&resource);
    if (converted == nullptr)
    {
        throw Error(std::string("RHI submit received ") + what + " from a different backend");
    }
    return *converted;
}

} // namespace

MetalCommandContext::~MetalCommandContext()
{
    if (m_encoder)
    {
        m_encoder->endEncoding();
    }
}

void MetalCommandContext::begin_pass(const RHIRenderPassDesc& pass)
{
    NS::SharedPtr<MTL::RenderPassDescriptor> descriptor = NS::RetainPtr(MTL::RenderPassDescriptor::renderPassDescriptor());
    for (uint32_t i = 0; i < pass.colour_count; ++i)
    {
        const RHIColourAttachment& colour = pass.colour[i];
        MetalRenderTarget& target = require_backend<MetalRenderTarget>(*colour.target, "a render target");
        MTL::RenderPassColorAttachmentDescriptor* attachment = descriptor->colorAttachments()->object(i);
        attachment->setTexture(target.mtl());
        attachment->setLoadAction(to_mtl(colour.load));
        attachment->setStoreAction(to_mtl(colour.store));
        attachment->setClearColor(MTL::ClearColor(colour.clear_colour.r, colour.clear_colour.g, colour.clear_colour.b, colour.clear_colour.a));
    }
    if (pass.depth.texture != nullptr)
    {
        MetalTexture& depth = require_backend<MetalTexture>(*pass.depth.texture, "a depth texture");
        MTL::RenderPassDepthAttachmentDescriptor* attachment = descriptor->depthAttachment();
        attachment->setTexture(depth.mtl());
        attachment->setLoadAction(to_mtl(pass.depth.load));
        attachment->setStoreAction(to_mtl(pass.depth.store));
        attachment->setClearDepth(pass.depth.clear_depth);
    }
    m_encoder = require_object(NS::RetainPtr(m_commands.renderCommandEncoder(descriptor.get())), "a render encoder");
    m_pipeline = nullptr;
}

void MetalCommandContext::end_pass()
{
    m_encoder->endEncoding();
    m_encoder.reset();
    m_pipeline = nullptr;
    m_index_buffer = nullptr;
}

void MetalCommandContext::copy_buffer(RHIBuffer& source, uint32_t source_offset, RHIBuffer& destination, uint32_t destination_offset, uint32_t size)
{
    MetalBuffer& from = require_backend<MetalBuffer>(source, "a buffer");
    MetalBuffer& to = require_backend<MetalBuffer>(destination, "a buffer");
    MTL::BlitCommandEncoder* blit = m_commands.blitCommandEncoder();
    blit->copyFromBuffer(from.mtl(), source_offset, to.mtl(), destination_offset, size);
    blit->endEncoding();
}

void MetalCommandContext::set_pipeline(RHIGraphicsPipeline& pipeline)
{
    m_pipeline = &require_backend<MetalPipeline>(pipeline, "a pipeline");
    const MetalRasterState& raster = m_pipeline->raster();
    m_encoder->setRenderPipelineState(m_pipeline->state());
    m_encoder->setDepthStencilState(m_pipeline->depth_stencil());
    m_encoder->setCullMode(raster.cull);
    m_encoder->setFrontFacingWinding(raster.winding);
    m_encoder->setTriangleFillMode(raster.fill);
}

void MetalCommandContext::set_viewport(const RHIViewportState& viewport)
{
    m_encoder->setViewport(MTL::Viewport{ viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth });
}

void MetalCommandContext::set_scissor(const RHIScissorRect& scissor)
{
    m_encoder->setScissorRect(MTL::ScissorRect{ static_cast<NS::UInteger>(scissor.x), static_cast<NS::UInteger>(scissor.y), scissor.width, scissor.height });
}

void MetalCommandContext::set_vertex_buffer(uint32_t slot, RHIBuffer& buffer, uint32_t offset)
{
    m_encoder->setVertexBuffer(require_backend<MetalBuffer>(buffer, "a buffer").mtl(), offset, METAL_VERTEX_STREAM_BASE + slot);
}

void MetalCommandContext::set_index_buffer(RHIBuffer& buffer, uint32_t offset, bool index32)
{
    m_index_buffer = require_backend<MetalBuffer>(buffer, "a buffer").mtl();
    m_index_offset = offset;
    m_index_type = index32 ? MTL::IndexTypeUInt32 : MTL::IndexTypeUInt16;
}

void MetalCommandContext::set_constants(RHIBindingId binding, const uint8_t* data, uint32_t size)
{
    const RHIBindingDesc& layout = m_pipeline->binding(binding);
    if (has_flag(layout.stage_mask, RHIShaderStageMask::Vertex))
    {
        m_encoder->setVertexBytes(data, size, layout.slot);
    }
    if (has_flag(layout.stage_mask, RHIShaderStageMask::Pixel))
    {
        m_encoder->setFragmentBytes(data, size, layout.slot);
    }
}

void MetalCommandContext::bind_buffer(RHIBindingId binding, RHIBuffer& buffer, uint32_t offset, uint32_t)
{
    const RHIBindingDesc& layout = m_pipeline->binding(binding);
    MTL::Buffer* mtl = require_backend<MetalBuffer>(buffer, "a buffer").mtl();
    if (has_flag(layout.stage_mask, RHIShaderStageMask::Vertex))
    {
        m_encoder->setVertexBuffer(mtl, offset, layout.slot);
    }
    if (has_flag(layout.stage_mask, RHIShaderStageMask::Pixel))
    {
        m_encoder->setFragmentBuffer(mtl, offset, layout.slot);
    }
}

void MetalCommandContext::bind_texture(RHIBindingId binding, RHITexture& texture, uint32_t array_index)
{
    const RHIBindingDesc& layout = m_pipeline->binding(binding);
    MTL::Texture* mtl = require_backend<MetalTexture>(texture, "a texture").mtl();
    if (has_flag(layout.stage_mask, RHIShaderStageMask::Vertex))
    {
        m_encoder->setVertexTexture(mtl, layout.slot + array_index);
    }
    if (has_flag(layout.stage_mask, RHIShaderStageMask::Pixel))
    {
        m_encoder->setFragmentTexture(mtl, layout.slot + array_index);
    }
}

void MetalCommandContext::bind_sampler(RHIBindingId binding, RHISampler& sampler, uint32_t array_index)
{
    const RHIBindingDesc& layout = m_pipeline->binding(binding);
    MTL::SamplerState* mtl = require_backend<MetalSampler>(sampler, "a sampler").mtl();
    if (has_flag(layout.stage_mask, RHIShaderStageMask::Vertex))
    {
        m_encoder->setVertexSamplerState(mtl, layout.slot + array_index);
    }
    if (has_flag(layout.stage_mask, RHIShaderStageMask::Pixel))
    {
        m_encoder->setFragmentSamplerState(mtl, layout.slot + array_index);
    }
}

void MetalCommandContext::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
    m_encoder->drawPrimitives(m_pipeline->raster().primitive, first_vertex, vertex_count, instance_count, first_instance);
}

void MetalCommandContext::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance)
{
    const uint32_t index_bytes = m_index_type == MTL::IndexTypeUInt32 ? 4 : 2;
    m_encoder->drawIndexedPrimitives(m_pipeline->raster().primitive, index_count, m_index_type, m_index_buffer, m_index_offset + first_index * index_bytes, instance_count, base_vertex, first_instance);
}

void MetalCommandContext::push_debug_group(const char* name)
{
    NS::String* label = NS::String::string(name, NS::UTF8StringEncoding);
    if (m_encoder)
    {
        m_encoder->pushDebugGroup(label);
    }
    else
    {
        m_commands.pushDebugGroup(label);
    }
}

void MetalCommandContext::pop_debug_group()
{
    if (m_encoder)
    {
        m_encoder->popDebugGroup();
    }
    else
    {
        m_commands.popDebugGroup();
    }
}

} // namespace oryx::metal
