#pragma once

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"
#include "Oryx/Graphics/Resources/IndexBuffer.h"
#include "Oryx/Graphics/Resources/RenderTarget.h"
#include "Oryx/Graphics/Resources/Texture2D.h"
#include "Oryx/Graphics/Resources/UniformBuffer.h"
#include "Oryx/Graphics/Resources/VertexBuffer.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Renderer/Batch/BatchRenderer.h"
#include "Oryx/Renderer/Scene/SceneRenderer.h"
#include "Oryx/Shaders/Cache/ShaderBinaryStore.h"
#include "Oryx/Renderer/PipelineDef.h"
#include "Oryx/Math/Vector2.h"
#include "Oryx/Renderer/Batch/DebugRenderer.h"
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
    // The pipeline of a pass definition for the back-buffer format, memoised; same lifetime as pipeline().
    [[nodiscard]] static GraphicsPipelineHandle pipeline(const PipelineDef& def, uint32_t permutation = 0);
    // The same for a pass with its own attachment formats (an offscreen target, a depth attachment).
    [[nodiscard]] static GraphicsPipelineHandle pipeline(const PipelineDef& def, const PassFormats& formats, uint32_t permutation = 0);
    // Throws Error for an invalid or stale handle.
    [[nodiscard]] static const GraphicsPipeline& resolve_pipeline(GraphicsPipelineHandle handle);
    [[nodiscard]] static GraphicsPipelineCacheStats pipeline_cache_stats();

    [[nodiscard]] static const RHITexturePtr& white_texture();
    [[nodiscard]] static const RHISamplerPtr& default_sampler();

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

    // Queues a draw for this frame into the Main pass, ahead of the scene's own stage output; end_frame records every queued item in submission order.
    static void submit(DrawItem item);

    // The frame's scene: begin_scene, submit sources, end_scene (SceneScope for RAII). GraphicsLayer opens one per frame around its clients.
    [[nodiscard]] static SceneRenderer& scene();

    // Records shapes from anywhere, between scenes too; the scene replays them at the end of its Scene2D stage and record_frame ages them.
    [[nodiscard]] static DebugRenderer& debug();
    // The built-in 8x8 bitmap font (see BuiltinFontSource); the debug renderer draws text with it until set_font replaces it.
    [[nodiscard]] static Font& default_font();
    // Counters of the frame being recorded; reset when the frame ring advances.
    [[nodiscard]] static const BatchStats& batch_stats();

    // Called only by GraphicsLayer: records, submits and presents the frame, then advances the frame ring. Returns whether anything was presented.
    static bool end_frame();

private:
    [[nodiscard]] static RendererContext& require_context();
};

} // namespace oryx
