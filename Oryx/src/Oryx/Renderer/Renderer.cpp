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
    register_shutdown_hook([] { Renderer::shutdown(); });
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

GraphicsPipelineHandle Renderer::builtin(BuiltinPipeline pipeline, uint32_t permutation)
{
    RendererContext& context = require_context();
    return context.builtin_pipelines.get(*context.rhi, context.pipelines, context.shaders, context.back_buffer_format, pipeline, permutation);
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
    require_context().items.push_back(std::move(item));
}

void Renderer::end_frame()
{
    record_frame(require_context());
}

} // namespace oryx
