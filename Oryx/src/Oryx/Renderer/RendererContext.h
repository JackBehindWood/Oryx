#pragma once

#include "Oryx/Graphics/RHI/RHICommandList.h"
#include "Oryx/Graphics/RHI/RHIViewport.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Renderer/Batch/BatchRenderer2D.h"
#include "Oryx/Renderer/Scene/SceneRenderer.h"
#include "Oryx/Renderer/PipelineDef.h"
#include "Oryx/Renderer/Batch/DebugRenderer.h"
#include "Oryx/Renderer/DefaultResources.h"
#include "Oryx/Renderer/DrawItem.h"
#include "Oryx/Renderer/FrameStats.h"
#include "Oryx/Renderer/GraphicsPipelineCache.h"
#include "Oryx/Renderer/Renderer.h"
#include "Oryx/Shaders/Cache/ShaderCache.h"
#include "Oryx/Shaders/ShaderLibrary.h"
#include "Oryx/Shaders/Source/ShaderSettings.h"

namespace oryx
{

// Everything the Renderer facade owns. Members are ordered by alignment and hot-before-cold, and so that implicit destruction runs
// dependants first and the device last; shutdown_renderer_context adds the GPU idle wait.
struct RendererContext
{
    UniquePtr<IRHI> rhi;
    RHIViewportPtr viewport;
    Colour clear_colour = { 0.08f, 0.08f, 0.1f, 1.0f };
    RHICommandList commands;
    GraphicsPipelineCache pipelines;
    PipelineMemo pipeline_memo;
    ShaderLibrary shaders;
    ShaderCache shader_cache;
    UniquePtr<ShaderSourceResolver> shader_sources;
    const IShaderBinaryStore* shader_store = nullptr;
    DefaultResources defaults;
    RHIFormat back_buffer_format = RHIFormat::BGRA8Unorm;
    // Declared before `debug`, which points at it until an application sets its own font.
    UniquePtr<Font> default_font;
    DebugRenderer debug;
    // Owns the frame's batcher and passes; declared after what it points at so it is destroyed first.
    UniquePtr<SceneRenderer> scene;
    FrameStats last_frame_stats;
    GraphicsPipelineCacheStats previous_pipelines;
    uint64_t previous_allocations = 0;
};

// Creates the device, compiles every registered shader and builds the default resources; throws Error on failure.
[[nodiscard]] UniquePtr<RendererContext> create_renderer_context(const RendererDesc& desc);
void shutdown_renderer_context(UniquePtr<RendererContext>& context);

// A batcher over the context's device, pipelines and defaults that writes into `sink`; its owner recycles it each frame (the scene does for its own).
[[nodiscard]] BatchRendererDesc batch_renderer_desc(RendererContext& context, std::vector<DrawItem>& sink);

// Both wait for the device to go idle first; every GraphicsPipelineHandle issued before release_pipelines becomes stale.
void release_pipelines(RendererContext& context);
void release_shader_cache(RendererContext& context);

// Recompiles every shader from the configured sources and rebuilds the pipelines; a failed compile throws Error and leaves the running shaders and pipelines untouched.
void reload_shaders(RendererContext& context);
void trim(RendererContext& context);

// Records the scene's passes in stage order, submits and presents, then advances the frame ring and recycles the batcher; clears the queued items.
// Throws Error while a scene is still open. Returns whether a back buffer was presented (false when the viewport is missing, zero-sized or hidden).
bool record_frame(RendererContext& context);

} // namespace oryx
