#include "oxpch.h"
#include "MetalCommandContext.h"

#include "MetalConvert.h"
#include "MetalResources.h"
#include "Oryx/Core/Error.h"

namespace oryx::metal
{

namespace
{

[[noreturn]] void not_yet(const char* what)
{
    throw Error(std::string("Metal ") + what + " is not implemented until Step 5");
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
    if (pass.colour_count != 1 || pass.depth.texture != nullptr)
    {
        not_yet("multiple render targets and depth attachments");
    }
    const RHIColourAttachment& colour = pass.colour[0];
    MetalRenderTarget* metal_target = dynamic_cast<MetalRenderTarget*>(colour.target);
    if (metal_target == nullptr)
    {
        throw Error("RHI submit received a render target from a different backend");
    }

    NS::SharedPtr<MTL::RenderPassDescriptor> descriptor = NS::RetainPtr(MTL::RenderPassDescriptor::renderPassDescriptor());
    MTL::RenderPassColorAttachmentDescriptor* attachment = descriptor->colorAttachments()->object(0);
    attachment->setTexture(metal_target->mtl());
    attachment->setLoadAction(colour.load == RHILoadAction::Clear ? MTL::LoadActionClear : colour.load == RHILoadAction::Load ? MTL::LoadActionLoad : MTL::LoadActionDontCare);
    attachment->setStoreAction(colour.store == RHIStoreAction::Store ? MTL::StoreActionStore : MTL::StoreActionDontCare);
    attachment->setClearColor(MTL::ClearColor(colour.clear_colour.r, colour.clear_colour.g, colour.clear_colour.b, colour.clear_colour.a));
    m_encoder = require_object(NS::RetainPtr(m_commands.renderCommandEncoder(descriptor.get())), "a render encoder");
}

void MetalCommandContext::end_pass()
{
    m_encoder->endEncoding();
    m_encoder.reset();
}

void MetalCommandContext::set_pipeline(RHIGraphicsPipeline&)
{
    not_yet("pipelines");
}

void MetalCommandContext::set_viewport(const RHIViewportState&) {}
void MetalCommandContext::set_scissor(const RHIScissorRect&) {}
void MetalCommandContext::set_vertex_buffer(uint32_t, RHIBuffer&, uint32_t) {}
void MetalCommandContext::set_index_buffer(RHIBuffer&, uint32_t, bool) {}
void MetalCommandContext::set_constants(RHIBindingId, const uint8_t*, uint32_t) {}
void MetalCommandContext::bind_buffer(RHIBindingId, RHIBuffer&, uint32_t, uint32_t) {}
void MetalCommandContext::bind_texture(RHIBindingId, RHITexture&, uint32_t) {}
void MetalCommandContext::bind_sampler(RHIBindingId, RHISampler&, uint32_t) {}
void MetalCommandContext::push_debug_group(const char*) {}
void MetalCommandContext::pop_debug_group() {}

void MetalCommandContext::draw(uint32_t, uint32_t, uint32_t, uint32_t)
{
    not_yet("draws");
}

void MetalCommandContext::draw_indexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t)
{
    not_yet("draws");
}

} // namespace oryx::metal
