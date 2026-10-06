#include "oxpch.h"
#include "Oryx/Renderer/FrameRecorder.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Renderer/RendererContext.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

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
    explicit ClearOnExit(SceneRenderer& scene)
        : m_scene(scene)
    {
    }

    ~ClearOnExit() { m_scene.clear_items(); }

    ClearOnExit(const ClearOnExit&) = delete;
    ClearOnExit& operator=(const ClearOnExit&) = delete;

private:
    SceneRenderer& m_scene;
};

const uint8_t k_zero_constants[RHI_MAX_CONSTANTS_SIZE] = {};

void bind_textures(RHICommandList& commands, const GraphicsPipeline& pipeline, const DrawItem& item, const DefaultResources& defaults)
{
    const RHIBindingId binding = pipeline.try_binding(SHADER_TEXTURES_BINDING);
    if (binding == RHI_INVALID_BINDING)
    {
        if (item.texture_count > 0 && !pipeline.is_fallback())
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
    bool has_vertices = false;
    for (const RHIBufferPtr& buffer : item.vertex_buffers)
    {
        has_vertices = has_vertices || static_cast<bool>(buffer);
    }
    if (!has_vertices)
    {
        throw Error("DrawItem needs a vertex buffer");
    }
    const GraphicsPipeline& pipeline = pipelines.resolve(item.pipeline);

    commands.set_pipeline(pipeline.rhi_ptr().get());
    for (uint32_t slot = 0; slot < RHI_MAX_VERTEX_SLOTS; ++slot)
    {
        if (item.vertex_buffers[slot])
        {
            commands.set_vertex_buffer(slot, item.vertex_buffers[slot].get(), item.vertex_offsets[slot]);
        }
    }
    if (item.index_buffer)
    {
        commands.set_index_buffer(item.index_buffer.get(), item.index_offset, item.index_type == IndexType::U32);
    }
    if (item.constants_size > 0)
    {
        commands.set_constants(pipeline.binding(SHADER_FRAME_BINDING), item.constants, item.constants_size);
    }
    else if (const RHIBindingId frame = pipeline.try_binding(SHADER_FRAME_BINDING); frame != RHI_INVALID_BINDING)
    {
        commands.set_constants(frame, k_zero_constants, pipeline.rhi().binding(frame).size);
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

namespace
{

RHIRenderPassDesc make_pass_desc(const RenderPass& pass, RHIRenderTarget* back_buffer, const Colour& main_clear)
{
    const RenderPassDesc& desc = pass.desc;
    RHIRenderPassDesc out;
    out.colour_count = 1;
    out.colour[0].target = desc.colour ? desc.colour.get() : back_buffer;
    out.colour[0].load = desc.colour_load;
    out.colour[0].store = desc.colour_store;
    out.colour[0].clear_colour = pass.id == RENDER_PASS_MAIN ? main_clear : desc.clear_colour;
    if (desc.depth)
    {
        out.depth = { desc.depth.get(), desc.depth_load, desc.depth_store, desc.clear_depth };
    }
    return out;
}

void record_pass(RendererContext& context, const RenderPass& pass, RHIRenderTarget* back_buffer)
{
    context.commands.begin_pass(make_pass_desc(pass, back_buffer, context.clear_colour));
    {
        RHIDebugScope scope(context.commands, pass.desc.name.c_str());
        uint32_t batch = 0;
        for (const DrawItem& item : pass.items)
        {
            try
            {
                record_draw_item(context.commands, item, context.pipelines, context.defaults);
            }
            catch (const Error& error)
            {
                if (typeid(error) != typeid(Error))
                {
                    throw;
                }
                const std::string where = pass.id == RENDER_PASS_MAIN ? "Draw Batch " : "Pass '" + pass.desc.name + "', Draw Batch ";
                throw Error(std::string(error.what()) + " [" + where + std::to_string(batch) + "]", error.detail());
            }
            ++batch;
        }
    }
    context.commands.end_pass();
}

} // namespace

bool record_frame(RendererContext& context)
{
    SceneRenderer& scene = *context.scene;
    if (scene.open() || scene.batcher_2d().open())
    {
        throw Error("A scene is still open at the end of the frame", "call end_scene before the frame is recorded");
    }
    ClearOnExit clear_items(scene);

    RHIRenderTargetPtr back_buffer;
    if (context.viewport)
    {
        back_buffer = context.viewport->acquire_back_buffer();
    }
    const bool presented = static_cast<bool>(back_buffer);
    if (back_buffer)
    {
        BackBufferGuard guard(*context.viewport);
        context.commands.clear();
        {
            RHIDebugScope frame(context.commands, "Frame");
            bool main_recorded = false;
            for (const RenderPass* pass : scene.ordered_passes())
            {
                const bool is_main = pass->id == RENDER_PASS_MAIN;
                main_recorded = main_recorded || is_main;
                if (pass->items.empty() && !is_main)
                {
                    continue;
                }
                record_pass(context, *pass, back_buffer.get());
            }
            if (!main_recorded)
            {
                record_pass(context, scene.pass(RENDER_PASS_MAIN), back_buffer.get());
            }
        }
        context.rhi->submit(context.commands);
        guard.dismiss();
        context.rhi->present(context.viewport.get());
    }
    context.rhi->end_frame();
    scene.finish_frame(context.rhi->frame_slot());
    context.debug.end_frame();
    return presented;
}

} // namespace oryx
