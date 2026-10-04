#pragma once

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"
#include "Oryx/Graphics/Resources/IndexBuffer.h"
#include "Oryx/Graphics/Resources/RenderTarget.h"
#include "Oryx/Graphics/Resources/Texture2D.h"
#include "Oryx/Graphics/Resources/UniformBuffer.h"
#include "Oryx/Graphics/Resources/VertexBuffer.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Renderer/DrawItem.h"
#include "Oryx/Renderer/GraphicsPipelineCache.h"
#include "Oryx/Shaders/ShaderLibrary.h"

namespace oryx
{

struct RendererContext;

struct RendererDesc
{
    RHIBackend backend = default_rhi_backend();
    RHIFormat back_buffer_format = RHIFormat::BGRA8Unorm;
};

// The only static renderer state: it forwards to one RendererContext that owns the device, shaders, pipelines and default resources. Main thread only.
class Renderer
{
public:
    static void init(const RendererDesc& desc = {});
    static void shutdown();
    [[nodiscard]] static bool initialised();

    [[nodiscard]] static IRHI& rhi();
    [[nodiscard]] static uint32_t frame_slot();
    // The format GraphicsLayer creates its viewport with; pipelines that draw into the back buffer target it.
    [[nodiscard]] static RHIFormat back_buffer_format();

    // Every registered shader permutation, compiled once by init.
    [[nodiscard]] static const ShaderLibrary& shaders();
    // Returns the cached pipeline for the description, creating it on first use. The handle stays valid until release_pipelines or trim.
    [[nodiscard]] static GraphicsPipelineHandle pipeline(const GraphicsPipelineDesc& desc);
    // Throws Error for an invalid or stale handle.
    [[nodiscard]] static const GraphicsPipeline& resolve_pipeline(GraphicsPipelineHandle handle);
    [[nodiscard]] static GraphicsPipelineCacheStats pipeline_cache_stats();

    [[nodiscard]] static const RHITexturePtr& white_texture();
    [[nodiscard]] static const RHISamplerPtr& default_sampler();

    [[nodiscard]] static VertexBuffer create_vertex_buffer(const VertexLayout& layout, uint32_t capacity, BufferMode mode) { return VertexBuffer::create(rhi(), layout, capacity, mode); }
    [[nodiscard]] static IndexBuffer create_index_buffer(IndexType type, uint32_t capacity, BufferMode mode) { return IndexBuffer::create(rhi(), type, capacity, mode); }
    [[nodiscard]] static UniformBuffer create_uniform_buffer(uint32_t size) { return UniformBuffer::create(rhi(), size); }
    [[nodiscard]] static Texture2D create_texture_2d(const Texture2DDesc& desc) { return Texture2D::create(rhi(), desc); }
    [[nodiscard]] static RenderTarget create_render_target(uint32_t width, uint32_t height, RHIFormat format = RHIFormat::RGBA8Unorm) { return RenderTarget::create(rhi(), width, height, format); }

    // Frees memory that can be rebuilt on demand (bulk teardown, mode change, memory pressure); queued draws and held resources are unaffected.
    // release_pipelines makes every GraphicsPipelineHandle stale, so holders acquire their pipelines again afterwards.
    static void release_pipelines();
    static void release_shader_cache();
    static void trim();

    static void set_clear_colour(const Colour& colour);
    [[nodiscard]] static const Colour& clear_colour();
    static void set_viewport(RHIViewportPtr viewport);

    // Queues a draw for this frame; GraphicsLayer's end_frame records every queued item into the back-buffer pass, in submission order.
    static void submit(DrawItem item);

    // Called only by GraphicsLayer: records, submits and presents the frame, then advances the frame ring.
    static void end_frame();

private:
    [[nodiscard]] static RendererContext& require_context();
};

} // namespace oryx
