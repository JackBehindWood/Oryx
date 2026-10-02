#pragma once

#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIPipeline.h"
#include "Oryx/Graphics/RHI/RHIRenderTarget.h"
#include "Oryx/Graphics/RHI/RHISampler.h"
#include "Oryx/Graphics/RHI/RHITexture.h"
#include "Oryx/Math/Colour.h"

namespace oryx
{

using RHIBindingId = uint32_t;

inline constexpr uint32_t RHI_MAX_VERTEX_SLOTS = 4;

struct RHIClear
{
    Colour colour;
    bool clear = true;
};

// What a backend implements to consume a recorded RHICommandList; each command calls exactly one method.
class IRHICommandContext
{
public:
    virtual ~IRHICommandContext() = default;

    virtual void begin_pass(RHIRenderTarget& target, const RHIClear& clear) = 0;
    virtual void set_pipeline(RHIGraphicsPipeline& pipeline) = 0;
    virtual void set_vertex_buffer(uint32_t slot, RHIBuffer& buffer, uint32_t offset) = 0;
    virtual void set_index_buffer(RHIBuffer& buffer, uint32_t offset, bool index32) = 0;
    virtual void bind_buffer(RHIBindingId binding, RHIBuffer& buffer) = 0;
    virtual void bind_texture(RHIBindingId binding, RHITexture& texture) = 0;
    virtual void bind_sampler(RHIBindingId binding, RHISampler& sampler) = 0;
    virtual void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex) = 0;
    virtual void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index) = 0;
    virtual void end_pass() = 0;
};

} // namespace oryx
