#include "oxpch.h"
#include "Oryx/Renderer/Renderer.h"

#include "Oryx/Core/Application.h"

namespace oryx
{

void Renderer::shutdown_context(UniquePtr<Context>& context)
{
    if (!context)
    {
        return;
    }
    context->commands.clear();
    context->viewport.reset();
    context->rhi->wait_idle();
    context.reset();
}

void Renderer::init(const RendererDesc& desc)
{
    if (initialised())
    {
        throw Error("Renderer is already initialised");
    }
    UniquePtr<Context> created = create_unique<Context>();
    created->rhi = create_rhi(desc.backend);
    context() = std::move(created);
    register_shutdown_hook([] { Renderer::shutdown(); });
}

void Renderer::shutdown()
{
    shutdown_context(context());
}

void Renderer::end_frame()
{
    Context& renderer = require_context();
    std::vector<Colour> clears = std::move(renderer.clears);
    renderer.clears.clear();
    if (clears.empty() || !renderer.viewport)
    {
        return;
    }

    RHIRenderTargetPtr back_buffer = renderer.viewport->acquire_back_buffer();
    if (!back_buffer)
    {
        return;
    }
    renderer.commands.clear();
    for (const Colour& colour : clears)
    {
        renderer.commands.begin_pass(back_buffer.get(), { colour, true });
        renderer.commands.end_pass();
    }
    renderer.rhi->submit(renderer.commands);
    renderer.rhi->present(*renderer.viewport);
}

} // namespace oryx
