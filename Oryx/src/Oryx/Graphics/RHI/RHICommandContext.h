#pragma once

#include "Oryx/Graphics/RHI/RHIBinding.h"
#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIPipeline.h"
#include "Oryx/Graphics/RHI/RHIRenderPass.h"
#include "Oryx/Graphics/RHI/RHISampler.h"
#include "Oryx/Graphics/RHI/RHITexture.h"

namespace oryx
{

// What a backend implements to consume a recorded RHICommandList; each command calls exactly one method.
class IRHICommandContext
{
public:
    virtual ~IRHICommandContext() = default;

    virtual void begin_pass(const RHIRenderPassDesc& pass) = 0;
    virtual void set_pipeline(RHIGraphicsPipeline& pipeline) = 0;
    virtual void set_viewport(const RHIViewportState& viewport) = 0;
    virtual void set_scissor(const RHIScissorRect& scissor) = 0;
    virtual void set_vertex_buffer(uint32_t slot, RHIBuffer& buffer, uint32_t offset) = 0;
    virtual void set_index_buffer(RHIBuffer& buffer, uint32_t offset, bool index32) = 0;
    virtual void set_constants(RHIBindingId binding, const uint8_t* data, uint32_t size) = 0;
    virtual void bind_buffer(RHIBindingId binding, RHIBuffer& buffer, uint32_t offset, uint32_t size) = 0;
    virtual void bind_texture(RHIBindingId binding, RHITexture& texture, uint32_t array_index) = 0;
    virtual void bind_sampler(RHIBindingId binding, RHISampler& sampler, uint32_t array_index) = 0;
    virtual void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) = 0;
    virtual void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance) = 0;
    virtual void push_debug_group(const char* name) = 0;
    virtual void pop_debug_group() = 0;
    virtual void end_pass() = 0;
};

} // namespace oryx
