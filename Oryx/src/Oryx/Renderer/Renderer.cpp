#include "oxpch.h"
#include "Oryx/Renderer/Renderer.h"

#include "Oryx/Core/Application.h"

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

} // namespace

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

    RHIRenderTargetPtr back_buffer;
    if (!clears.empty() && renderer.viewport)
    {
        back_buffer = renderer.viewport->acquire_back_buffer();
    }
    if (back_buffer)
    {
        BackBufferGuard guard(*renderer.viewport);
        renderer.commands.clear();
        for (const Colour& colour : clears)
        {
            renderer.commands.begin_pass(back_buffer.get(), { colour, true });
            renderer.commands.end_pass();
        }
        renderer.rhi->submit(renderer.commands);
        guard.dismiss();
        renderer.rhi->present(*renderer.viewport);
    }
    renderer.rhi->end_frame();
}

} // namespace oryx
