#pragma once

#include "MetalApi.h"
#include "MetalResources.h"
#include "Oryx/Graphics/RHI/RHICommandContext.h"

namespace oryx::metal
{

// Encodes one RHICommandList into one command buffer. Holds only raw pointers and scalars: the list retains every resource, and nothing allocates per draw.
class MetalCommandContext final : public IRHICommandContext
{
public:
    explicit MetalCommandContext(MTL::CommandBuffer& commands)
        : m_commands(commands)
    {
    }
    ~MetalCommandContext() override;

    void begin_pass(const RHIRenderPassDesc& pass) override;
    void set_pipeline(RHIGraphicsPipeline& pipeline) override;
    void set_viewport(const RHIViewportState& viewport) override;
    void set_scissor(const RHIScissorRect& scissor) override;
    void set_vertex_buffer(uint32_t slot, RHIBuffer& buffer, uint32_t offset) override;
    void set_index_buffer(RHIBuffer& buffer, uint32_t offset, bool index32) override;
    void set_constants(RHIBindingId binding, const uint8_t* data, uint32_t size) override;
    void bind_buffer(RHIBindingId binding, RHIBuffer& buffer, uint32_t offset, uint32_t size) override;
    void bind_texture(RHIBindingId binding, RHITexture& texture, uint32_t array_index) override;
    void bind_sampler(RHIBindingId binding, RHISampler& sampler, uint32_t array_index) override;
    void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) override;
    void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance) override;
    void push_debug_group(const char* name) override;
    void pop_debug_group() override;
    void end_pass() override;
    void copy_buffer(RHIBuffer& source, uint32_t source_offset, RHIBuffer& destination, uint32_t destination_offset, uint32_t size) override;

private:
    MTL::CommandBuffer& m_commands;
    NS::SharedPtr<MTL::RenderCommandEncoder> m_encoder;
    const MetalPipeline* m_pipeline = nullptr;
    uint32_t m_target_width = 0;
    uint32_t m_target_height = 0;
    MTL::Buffer* m_index_buffer = nullptr;
    uint32_t m_index_offset = 0;
    MTL::IndexType m_index_type = MTL::IndexTypeUInt16;
};

} // namespace oryx::metal
