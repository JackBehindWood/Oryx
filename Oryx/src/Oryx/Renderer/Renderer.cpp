#include "oxpch.h"
#include "Oryx/Renderer/Renderer.h"

#include "Oryx/Core/Application.h"
#include "Oryx/Renderer/RendererContext.h"

namespace oryx
{

namespace
{

UniquePtr<RendererContext>& context_storage()
{
    static UniquePtr<RendererContext> instance;
    return instance;
}

// Application's shutdown consumes the hooks, a direct Renderer::shutdown() does not; the flag follows whichever is still pending.
bool& shutdown_hook_registered()
{
    static bool registered = false;
    return registered;
}

} // namespace

RendererContext& Renderer::require_context()
{
    if (!initialised())
    {
        throw Error("Renderer is not initialised", "call Renderer::init first");
    }
    return *context_storage();
}

void Renderer::init(const RendererDesc& desc)
{
    if (initialised())
    {
        throw Error("Renderer is already initialised");
    }
    context_storage() = create_renderer_context(desc);
    if (!shutdown_hook_registered())
    {
        shutdown_hook_registered() = true;
        register_shutdown_hook([]
        {
            shutdown_hook_registered() = false;
            Renderer::shutdown();
        });
    }
}

void Renderer::shutdown()
{
    shutdown_renderer_context(context_storage());
}

bool Renderer::initialised()
{
    return context_storage() != nullptr;
}

IRHI& Renderer::rhi()
{
    return *require_context().rhi;
}

uint32_t Renderer::frame_slot()
{
    return rhi().frame_slot();
}

RHIFormat Renderer::back_buffer_format()
{
    return require_context().back_buffer_format;
}

const ShaderLibrary& Renderer::shaders()
{
    return require_context().shaders;
}

GraphicsPipelineHandle Renderer::pipeline(const GraphicsPipelineDesc& desc)
{
    RendererContext& context = require_context();
    return context.pipelines.get_or_create(*context.rhi, desc);
}

GraphicsPipelineHandle Renderer::pipeline(const PipelineDef& def, uint32_t permutation)
{
    RendererContext& context = require_context();
    return context.pipeline_memo.get(*context.rhi, context.pipelines, context.shaders, context.back_buffer_format, def, permutation);
}

GraphicsPipelineHandle Renderer::pipeline(const PipelineDef& def, const PassFormats& formats, uint32_t permutation)
{
    RendererContext& context = require_context();
    return context.pipeline_memo.get(*context.rhi, context.pipelines, context.shaders, formats, def, permutation);
}

const GraphicsPipeline& Renderer::resolve_pipeline(GraphicsPipelineHandle handle)
{
    return require_context().pipelines.resolve(handle);
}

GraphicsPipelineCacheStats Renderer::pipeline_cache_stats()
{
    return require_context().pipelines.stats();
}

const RHITexturePtr& Renderer::white_texture()
{
    return require_context().defaults.white_texture;
}

const RHISamplerPtr& Renderer::default_sampler()
{
    return require_context().defaults.sampler;
}

void Renderer::release_pipelines()
{
    oryx::release_pipelines(require_context());
}

void Renderer::release_shader_cache()
{
    oryx::release_shader_cache(require_context());
}

void Renderer::reload_shaders()
{
    oryx::reload_shaders(require_context());
}

void Renderer::trim()
{
    oryx::trim(require_context());
}

void Renderer::set_clear_colour(const Colour& colour)
{
    require_context().clear_colour = colour;
}

const Colour& Renderer::clear_colour()
{
    return require_context().clear_colour;
}

void Renderer::set_viewport(RHIViewportPtr viewport)
{
    require_context().viewport = std::move(viewport);
}

void Renderer::submit(DrawItem item)
{
    require_context().scene->pass(RENDER_PASS_MAIN).items.push_back(std::move(item));
}

SceneRenderer& Renderer::scene()
{
    return *require_context().scene;
}

DebugRenderer& Renderer::debug()
{
    return require_context().debug;
}

Font& Renderer::default_font()
{
    return *require_context().default_font;
}

const BatchStats& Renderer::batch_stats()
{
    return require_context().scene->batcher_2d().stats();
}

bool Renderer::end_frame()
{
    return record_frame(require_context());
}

} // namespace oryx
