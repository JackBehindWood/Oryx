#pragma once

#include "Oryx/Graphics/RHI/RHICommandList.h"
#include "Oryx/Graphics/RHI/RHIViewport.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Renderer/BatchRenderer2D.h"
#include "Oryx/Renderer/BuiltinPipelines.h"
#include "Oryx/Renderer/DebugRenderer.h"
#include "Oryx/Renderer/DefaultResources.h"
#include "Oryx/Renderer/DrawItem.h"
#include "Oryx/Renderer/GraphicsPipelineCache.h"
#include "Oryx/Renderer/Renderer.h"
#include "Oryx/Shaders/ShaderCache.h"
#include "Oryx/Shaders/ShaderLibrary.h"

namespace oryx
{

// Everything the Renderer facade owns. Members are ordered by alignment and hot-before-cold, and so that implicit destruction runs
// dependants first and the device last; shutdown_renderer_context adds the GPU idle wait.
struct RendererContext
{
    UniquePtr<IRHI> rhi;
    RHIViewportPtr viewport;
    Colour clear_colour = { 0.08f, 0.08f, 0.1f, 1.0f };
    std::vector<DrawItem> items;
    RHICommandList commands;
    GraphicsPipelineCache pipelines;
    BuiltinPipelines builtin_pipelines;
    ShaderLibrary shaders;
    ShaderCache shader_cache;
    DefaultResources defaults;
    RHIFormat back_buffer_format = RHIFormat::BGRA8Unorm;
    // Batchers recycled once per frame by record_frame; the first is the facade's own.
    std::vector<BatchRenderer*> batchers;
    UniquePtr<BatchRenderer2D> batcher;
    // Declared before `debug`, which points at it until an application sets its own font.
    UniquePtr<Font> default_font;
    DebugRenderer debug;
};

// Creates the device, compiles every registered shader and builds the default resources; throws Error on failure.
[[nodiscard]] UniquePtr<RendererContext> create_renderer_context(const RendererDesc& desc);
void shutdown_renderer_context(UniquePtr<RendererContext>& context);

// A batcher over the context's device, pipelines and defaults that writes into `sink`; the caller registers it in `batchers` if record_frame should recycle it.
[[nodiscard]] BatchRendererDesc batch_renderer_desc(RendererContext& context, std::vector<DrawItem>& sink);

// Both wait for the device to go idle first; every GraphicsPipelineHandle issued before release_pipelines becomes stale.
void release_pipelines(RendererContext& context);
void release_shader_cache(RendererContext& context);
void trim(RendererContext& context);

// Records the queued items into the back-buffer pass, submits and presents, then advances the frame ring and recycles the batchers; clears the queue.
// Throws Error while a batcher scene is still open.
void record_frame(RendererContext& context);

} // namespace oryx
