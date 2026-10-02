#include "oxpch.h"
#include "Oryx/Renderer/GraphicsLayer.h"

#include "Oryx/Core/Application.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Events/WindowEvent.h"
#include "Oryx/Renderer/Renderer.h"

namespace oryx
{

GraphicsLayer::GraphicsLayer()
    : Layer("GraphicsLayer")
{
}

void GraphicsLayer::attach()
{
    m_window = Application::Get().window();
    if (!m_window)
    {
        throw Error("GraphicsLayer needs a window", "call Application::create_window before pushing it");
    }

    NativeWindowHandle handle = m_window->native_handle();
    m_width = handle.framebuffer_width;
    m_height = handle.framebuffer_height;

    if (Renderer::initialised())
    {
        IRHI& rhi = Renderer::rhi();
        RHIViewportDesc desc;
        desc.native_window = rhi.backend() == RHIBackend::Null ? nullptr : handle.native;
        desc.width = static_cast<uint32_t>(handle.framebuffer_width);
        desc.height = static_cast<uint32_t>(handle.framebuffer_height);
        desc.scale = handle.content_scale;
        m_viewport = rhi.create_viewport(desc);
        Renderer::set_viewport(m_viewport);
    }
}

void GraphicsLayer::detach()
{
    if (Renderer::initialised())
    {
        Renderer::set_viewport({});
    }
    m_viewport.reset();
}

void GraphicsLayer::update(double)
{
    if (m_viewport)
    {
        Renderer::end_frame();
    }
    m_window->poll_events();
    if (m_window->should_close())
    {
        Application::Get().close();
    }
}

void GraphicsLayer::event(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<WindowCloseEvent>([](WindowCloseEvent&)
    {
        Application::Get().close();
        return false;
    });
    dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& resize)
    {
        m_width = resize.width();
        m_height = resize.height();
        if (m_viewport)
        {
            NativeWindowHandle handle = m_window->native_handle();
            Renderer::rhi().resize_viewport(*m_viewport, static_cast<uint32_t>(handle.framebuffer_width), static_cast<uint32_t>(handle.framebuffer_height), handle.content_scale);
        }
        return false;
    });
}

} // namespace oryx
