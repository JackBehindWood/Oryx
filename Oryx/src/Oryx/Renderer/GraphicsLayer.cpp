#include "oxpch.h"
#include "Oryx/Renderer/GraphicsLayer.h"

#include "Oryx/Core/Application.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Events/WindowEvent.h"

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
}

void GraphicsLayer::update(double)
{
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
        return true;
    });
    dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& resize)
    {
        m_width = resize.width();
        m_height = resize.height();
        return false;
    });
}

} // namespace oryx
