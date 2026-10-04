#include "oxpch.h"
#include "Oryx/Renderer/FrameRecorder.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Renderer/RendererContext.h"
#include "Oryx/Shaders/Static/StaticShaders.h"

namespace oryx
{

namespace
{

class BackBufferGuard
{
public:
    explicit BackBufferGuard(RHIViewport& viewport)
        : m_viewport(viewport)
    {
    }

    ~BackBufferGuard()
    {
        if (!m_dismissed)
        {
            m_viewport.discard_back_buffer();
        }
    }

    BackBufferGuard(const BackBufferGuard&) = delete;
    BackBufferGuard& operator=(const BackBufferGuard&) = delete;

    void dismiss() { m_dismissed = true; }

private:
    RHIViewport& m_viewport;
    bool m_dismissed = false;
};

class ClearOnExit
{
public:
    explicit ClearOnExit(std::vector<DrawItem>& items)
        : m_items(items)
    {
    }

    ~ClearOnExit() { m_items.clear(); }

    ClearOnExit(const ClearOnExit&) = delete;
    ClearOnExit& operator=(const ClearOnExit&) = delete;

private:
    std::vector<DrawItem>& m_items;
};

void bind_textures(RHICommandList& commands, const GraphicsPipeline& pipeline, const DrawItem& item, const DefaultResources& defaults)
{
    const RHIBindingId binding = pipeline.try_binding(SHADER_TEXTURES_BINDING);
    if (binding == RHI_INVALID_BINDING)
    {
        if (item.texture_count > 0)
        {
            throw Error("DrawItem has textures but its pipeline has no texture binding");
        }
        return;
    }
    const uint32_t slots = pipeline.rhi().binding(binding).array_count;
    if (item.texture_count > slots)
    {
        throw Error("DrawItem has more textures than the pipeline's texture array", std::to_string(item.texture_count) + " > " + std::to_string(slots));
    }
    for (uint32_t i = 0; i < slots; ++i)
    {
        const RHITexturePtr& texture = i < item.texture_count ? item.textures[i] : defaults.white_texture;
        commands.bind_texture(binding, texture.get(), i);
    }
}

} // namespace

void record_draw_item(RHICommandList& commands, const DrawItem& item, const GraphicsPipelineCache& pipelines, const DefaultResources& defaults)
{
    if (!item.vertex_buffer)
    {
        throw Error("DrawItem needs a vertex buffer");
    }
    const GraphicsPipeline& pipeline = pipelines.resolve(item.pipeline);

    commands.set_pipeline(pipeline.rhi_ptr().get());
    commands.set_vertex_buffer(0, item.vertex_buffer.get(), item.vertex_offset);
    if (item.index_buffer)
    {
        commands.set_index_buffer(item.index_buffer.get(), item.index_offset, item.index_type == IndexType::U32);
    }
    if (item.constants_size > 0)
    {
        commands.set_constants(pipeline.binding(SHADER_FRAME_BINDING), item.constants, item.constants_size);
    }
    bind_textures(commands, pipeline, item, defaults);
    if (item.sampler)
    {
        commands.bind_sampler(pipeline.binding(SHADER_SAMPLER_BINDING), item.sampler.get());
    }
    else
    {
        const RHIBindingId sampler = pipeline.try_binding(SHADER_SAMPLER_BINDING);
        if (sampler != RHI_INVALID_BINDING)
        {
            commands.bind_sampler(sampler, defaults.sampler.get());
        }
    }

    if (item.index_buffer)
    {
        commands.draw_indexed(item.index_count, item.instance_count, item.first, item.base_vertex, item.first_instance);
    }
    else
    {
        commands.draw(item.vertex_count, item.instance_count, item.first, item.first_instance);
    }
}

void record_frame(RendererContext& context)
{
    ClearOnExit clear_items(context.items);

    RHIRenderTargetPtr back_buffer;
    if (context.viewport)
    {
        back_buffer = context.viewport->acquire_back_buffer();
    }
    if (back_buffer)
    {
        BackBufferGuard guard(*context.viewport);
        context.commands.clear();
        context.commands.begin_pass(back_buffer.get(), { context.clear_colour, true });
        for (const DrawItem& item : context.items)
        {
            record_draw_item(context.commands, item, context.pipelines, context.defaults);
        }
        context.commands.end_pass();
        context.rhi->submit(context.commands);
        guard.dismiss();
        context.rhi->present(*context.viewport);
    }
    context.rhi->end_frame();
}

} // namespace oryx
