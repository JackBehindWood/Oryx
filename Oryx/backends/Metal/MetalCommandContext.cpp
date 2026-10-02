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

void MetalCommandContext::begin_pass(RHIRenderTarget& target, const RHIClear& clear)
{
    MetalRenderTarget* metal_target = dynamic_cast<MetalRenderTarget*>(&target);
    if (metal_target == nullptr)
    {
        throw Error("RHI submit received a render target from a different backend");
    }

    NS::SharedPtr<MTL::RenderPassDescriptor> pass = NS::RetainPtr(MTL::RenderPassDescriptor::renderPassDescriptor());
    MTL::RenderPassColorAttachmentDescriptor* attachment = pass->colorAttachments()->object(0);
    attachment->setTexture(metal_target->mtl());
    attachment->setLoadAction(clear.clear ? MTL::LoadActionClear : MTL::LoadActionLoad);
    attachment->setStoreAction(MTL::StoreActionStore);
    attachment->setClearColor(MTL::ClearColor(clear.colour.r, clear.colour.g, clear.colour.b, clear.colour.a));
    m_encoder = require_object(NS::RetainPtr(m_commands.renderCommandEncoder(pass.get())), "a render encoder");
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

// The binding model arrives with shader reflection in Step 5; until then a bind is validated by the list and has no effect.
void MetalCommandContext::set_vertex_buffer(uint32_t, RHIBuffer&, uint32_t) {}
void MetalCommandContext::set_index_buffer(RHIBuffer&, uint32_t, bool) {}
void MetalCommandContext::bind_buffer(RHIBindingId, RHIBuffer&) {}
void MetalCommandContext::bind_texture(RHIBindingId, RHITexture&) {}
void MetalCommandContext::bind_sampler(RHIBindingId, RHISampler&) {}

void MetalCommandContext::draw(uint32_t, uint32_t, uint32_t)
{
    not_yet("draws");
}

void MetalCommandContext::draw_indexed(uint32_t, uint32_t, uint32_t)
{
    not_yet("draws");
}

} // namespace oryx::metal
