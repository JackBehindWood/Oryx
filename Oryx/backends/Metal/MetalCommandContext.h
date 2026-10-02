#pragma once

#include "MetalApi.h"
#include "Oryx/Graphics/RHI/RHICommandContext.h"

namespace oryx::metal
{

// Encodes one RHICommandList into one command buffer. Pipelines and draws arrive with Step 5 and throw until then.
class MetalCommandContext final : public IRHICommandContext
{
public:
    explicit MetalCommandContext(MTL::CommandBuffer& commands)
        : m_commands(commands)
    {
    }
    ~MetalCommandContext() override;

    void begin_pass(RHIRenderTarget& target, const RHIClear& clear) override;
    void set_pipeline(RHIGraphicsPipeline& pipeline) override;
    void set_vertex_buffer(uint32_t slot, RHIBuffer& buffer, uint32_t offset) override;
    void set_index_buffer(RHIBuffer& buffer, uint32_t offset, bool index32) override;
    void bind_buffer(RHIBindingId binding, RHIBuffer& buffer) override;
    void bind_texture(RHIBindingId binding, RHITexture& texture) override;
    void bind_sampler(RHIBindingId binding, RHISampler& sampler) override;
    void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex) override;
    void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index) override;
    void end_pass() override;

private:
    MTL::CommandBuffer& m_commands;
    NS::SharedPtr<MTL::RenderCommandEncoder> m_encoder;
};

} // namespace oryx::metal
