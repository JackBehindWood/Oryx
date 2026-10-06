#pragma once

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"
#include "Oryx/Graphics/Resources/IndexBuffer.h"
#include "Oryx/Graphics/Resources/RenderTarget.h"
#include "Oryx/Graphics/Resources/Texture2D.h"
#include "Oryx/Graphics/Resources/UniformBuffer.h"
#include "Oryx/Graphics/Resources/VertexBuffer.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Renderer/BatchRenderer.h"
#include "Oryx/Shaders/ShaderBinaryStore.h"
#include "Oryx/Renderer/BuiltinPipelines.h"
#include "Oryx/Math/Vector2.h"
#include "Oryx/Renderer/DebugRenderer.h"
#include "Oryx/Renderer/DrawItem.h"
#include "Oryx/Renderer/Font.h"
#include "Oryx/Renderer/GraphicsPipelineCache.h"
#include "Oryx/Shaders/ShaderLibrary.h"

namespace oryx
{

struct RendererContext;

struct RendererDesc
{
    RHIBackend backend = default_rhi_backend();
    RHIFormat back_buffer_format = RHIFormat::BGRA8Unorm;
    // Persistent compiled-shader store, borrowed and required to outlive the renderer; null compiles every run and cannot load cooked shaders.
    const IShaderBinaryStore* shader_store = nullptr;
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
    // The engine's own pipeline for the back-buffer format; same lifetime as pipeline().
    [[nodiscard]] static GraphicsPipelineHandle builtin(BuiltinPipeline pipeline, uint32_t permutation = 0);
    // Throws Error for an invalid or stale handle.
    [[nodiscard]] static const GraphicsPipeline& resolve_pipeline(GraphicsPipelineHandle handle);
    [[nodiscard]] static GraphicsPipelineCacheStats pipeline_cache_stats();

    [[nodiscard]] static const RHITexturePtr& white_texture();
    [[nodiscard]] static const RHISamplerPtr& default_sampler();

    [[nodiscard]] static VertexBuffer create_vertex_buffer(const RHIVertexDeclaration& declaration, uint32_t capacity, BufferMode mode) { return VertexBuffer::create(rhi(), declaration, capacity, mode); }
    [[nodiscard]] static IndexBuffer create_index_buffer(IndexType type, uint32_t capacity, BufferMode mode) { return IndexBuffer::create(rhi(), type, capacity, mode); }
    [[nodiscard]] static UniformBuffer create_uniform_buffer(uint32_t size) { return UniformBuffer::create(rhi(), size); }
    [[nodiscard]] static Texture2D create_texture_2d(const Texture2DDesc& desc) { return Texture2D::create(rhi(), desc); }
    [[nodiscard]] static RenderTarget create_render_target(uint32_t width, uint32_t height, RHIFormat format = RHIFormat::RGBA8Unorm) { return RenderTarget::create(rhi(), width, height, format); }

    // Frees memory that can be rebuilt on demand (bulk teardown, mode change, memory pressure); queued draws and held resources are unaffected.
    // release_pipelines makes every GraphicsPipelineHandle stale, so holders acquire their pipelines again afterwards.
    static void release_pipelines();
    static void release_shader_cache();
    static void trim();
    // Recompiles every shader from its source (loose files in dev builds); a failure throws Error and keeps the running shaders. Makes pipeline handles stale on success.
    static void reload_shaders();

    static void set_clear_colour(const Colour& colour);
    [[nodiscard]] static const Colour& clear_colour();
    static void set_viewport(RHIViewportPtr viewport);

    // Queues a draw for this frame; GraphicsLayer's end_frame records every queued item into the back-buffer pass, in submission order.
    static void submit(DrawItem item);

    // Immediate-mode 2D drawing: primitives between begin_scene and end_scene are batched into DrawItems and submitted in call order.
    // draw_* outside a scene, a nested begin_scene and end_frame with a scene still open all throw Error. See BatchRenderer2D for the coordinate conventions.
    static void begin_scene(const Camera& camera);
    static void end_scene();
    static void flush();
    static void draw_triangle(const Vec2f& a, const Vec2f& b, const Vec2f& c, const Colour& colour);
    static void draw_triangle(const Vec2f& a, const Vec2f& b, const Vec2f& c, const Colour& colour_a, const Colour& colour_b, const Colour& colour_c);
    static void draw_line(const Vec2f& a, const Vec2f& b, const Colour& colour);
    static void draw_line(const Vec2f& a, const Vec2f& b, const Colour& colour_a, const Colour& colour_b);
    static void draw_quad(const Vec2f (&corners)[4], const Colour& colour);
    static void draw_rect(const Vec2f& position, const Vec2f& size, const Colour& colour, float rotation = 0.0f);
    static void draw_sprite(const Vec2f& position, const Vec2f& size, const Texture2D& texture, const Colour& tint = { 1.0f, 1.0f, 1.0f, 1.0f }, float rotation = 0.0f, const Vec2f& uv_min = { 0.0f, 0.0f }, const Vec2f& uv_max = { 1.0f, 1.0f });
    static void draw_circle(const Vec2f& centre, float radius, const Colour& colour, float thickness = 1.0f, float fade = 0.005f);
    static void draw_text(const Vec2f& position, std::string_view text, Font& font, const TextStyle& style = {});
    // Records shapes from anywhere, between scenes too; draw_debug replays them into the open scene and record_frame ages them.
    [[nodiscard]] static DebugRenderer& debug();
    // The built-in 8x8 bitmap font (see BuiltinFontSource); the debug renderer draws text with it until set_font replaces it.
    [[nodiscard]] static Font& default_font();
    static void draw_debug();
    // Counters of the frame being recorded; reset when the frame ring advances.
    [[nodiscard]] static const BatchStats& batch_stats();

    // Called only by GraphicsLayer: records, submits and presents the frame, then advances the frame ring.
    static void end_frame();

private:
    [[nodiscard]] static RendererContext& require_context();
};

} // namespace oryx
