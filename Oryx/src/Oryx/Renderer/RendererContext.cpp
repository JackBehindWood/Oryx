#include "oxpch.h"
#include "Oryx/Renderer/RendererContext.h"

#include "Oryx/Text/BuiltinFontSource.h"

namespace oryx
{

namespace
{

constexpr size_t INITIAL_ITEM_CAPACITY = 64;

} // namespace

UniquePtr<RendererContext> create_renderer_context(const RendererDesc& desc)
{
    UniquePtr<RendererContext> context = create_unique<RendererContext>();
    context->rhi = create_rhi(desc.backend);
    context->back_buffer_format = desc.back_buffer_format;
    context->items.reserve(INITIAL_ITEM_CAPACITY);
    context->shaders.compile_all(*context->rhi, context->shader_cache);
    context->defaults = create_default_resources(*context->rhi);
    context->batcher = create_unique<BatchRenderer2D>(batch_renderer_desc(*context, context->items));
    context->batchers.push_back(context->batcher.get());
    context->default_font = create_unique<Font>(Font::create(create_unique<BuiltinFontSource>()));
    context->debug.set_font(context->default_font.get());
    return context;
}

BatchRendererDesc batch_renderer_desc(RendererContext& context, std::vector<DrawItem>& sink)
{
    return { *context.rhi, context.pipelines, context.builtin_pipelines, context.shaders, context.defaults, sink, context.back_buffer_format };
}

void shutdown_renderer_context(UniquePtr<RendererContext>& context)
{
    if (!context)
    {
        return;
    }
    context->commands.clear();
    context->items.clear();
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
    context.builtin_pipelines.reset();
}

void release_shader_cache(RendererContext& context)
{
    context.shader_cache.clear();
}

void trim(RendererContext& context)
{
    release_pipelines(context);
    release_shader_cache(context);
}

} // namespace oryx
