#include "oxpch.h"
#include "Oryx/Renderer/RendererContext.h"

#include "Oryx/Text/BuiltinFontSource.h"

namespace oryx
{

namespace
{

void load_shaders(RendererContext& context, bool reload)
{
    if (context.shader_sources->mode() == ShaderSourceMode::Cooked)
    {
        if (context.shader_store == nullptr)
        {
            throw Error("cooked shaders need RendererDesc::shader_store");
        }
        context.shaders.load_cooked(*context.rhi, *context.shader_store);
    }
    else if (reload)
    {
        context.shaders.reload_all(*context.rhi, context.shader_cache, context.shader_sources->provider());
    }
    else
    {
        context.shaders.compile_all(*context.rhi, context.shader_cache, context.shader_sources->provider());
    }
}

} // namespace

UniquePtr<RendererContext> create_renderer_context(const RendererDesc& desc)
{
    UniquePtr<RendererContext> context = create_unique<RendererContext>();
    context->rhi = create_rhi(desc.backend);
    context->back_buffer_format = desc.back_buffer_format;
    const ShaderSettings& shader_settings = settings_of<ShaderSettings>();
    context->shader_sources = create_unique<ShaderSourceResolver>(shader_settings);
    context->shader_cache.set_slang_options({ shader_settings.slangc });
    context->shader_store = desc.shader_store;
    context->shader_cache.set_store(desc.shader_store);
    context->shaders.set_error_fallback(shader_settings.error_fallback && context->shader_sources->mode() != ShaderSourceMode::Cooked);
    load_shaders(*context, false);
    context->defaults = create_default_resources(*context->rhi);
    context->default_font = create_unique<Font>(Font::create(create_unique<BuiltinFontSource>()));
    context->debug.set_font(context->default_font.get());
    // The scene redirects its batcher into the Main pass, so the descriptor's sink is only a placeholder.
    std::vector<DrawItem> placeholder;
    context->scene = create_unique<SceneRenderer>(SceneRendererDesc{ batch_renderer_desc(*context, placeholder), &context->debug });
    return context;
}

BatchRendererDesc batch_renderer_desc(RendererContext& context, std::vector<DrawItem>& sink)
{
    return { *context.rhi, context.pipelines, context.pipeline_memo, context.shaders, context.defaults, sink, context.back_buffer_format };
}

void shutdown_renderer_context(UniquePtr<RendererContext>& context)
{
    if (!context)
    {
        return;
    }
    context->commands.clear();
    if (context->scene)
    {
        context->scene->clear_items();
    }
    context->viewport.reset();
    context->debug.clear();
    context->debug.set_font(nullptr);
    context->default_font.reset();
    context->rhi->wait_idle();
    context.reset();
}

void release_pipelines(RendererContext& context)
{
    context.rhi->wait_idle();
    context.pipelines.clear();
    context.pipeline_memo.reset();
}

void release_shader_cache(RendererContext& context)
{
    context.shader_cache.clear();
}

void reload_shaders(RendererContext& context)
{
    load_shaders(context, true);
    release_pipelines(context);
}

void trim(RendererContext& context)
{
    release_pipelines(context);
    release_shader_cache(context);
}

} // namespace oryx
