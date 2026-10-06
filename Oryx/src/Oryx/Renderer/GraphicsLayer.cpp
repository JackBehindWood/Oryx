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
    const GraphicsSettings& settings = settings_of<GraphicsSettings>();
    m_applied_vsync = settings.vsync;

    if (Renderer::initialised())
    {
        IRHI& rhi = Renderer::rhi();
        RHIViewportDesc desc;
        desc.native_window = rhi.backend() == RHIBackend::Null ? nullptr : handle.native;
        desc.width = static_cast<uint32_t>(handle.framebuffer_width);
        desc.height = static_cast<uint32_t>(handle.framebuffer_height);
        desc.scale = handle.content_scale;
        desc.format = Renderer::back_buffer_format();
        desc.vsync = settings.vsync;
        m_viewport = rhi.create_viewport(desc);
        Renderer::set_viewport(m_viewport);
    }

    apply_settings(settings);
    m_settings_subscription = on_settings_changed<GraphicsSettings>([this](const GraphicsSettings& changed) { apply_settings(changed); });
}

void GraphicsLayer::apply_settings(const GraphicsSettings& settings)
{
    m_idle_sleep = std::chrono::milliseconds(settings.idle_sleep_ms);
    m_frame_period = settings.max_fps > 0 ? std::chrono::nanoseconds(1'000'000'000 / settings.max_fps) : std::chrono::nanoseconds(0);
    m_next_frame = std::chrono::steady_clock::now();
    m_reload_key = key_from_name(settings.reload_key);
    if (m_viewport && Renderer::initialised() && settings.vsync != m_applied_vsync)
    {
        Renderer::rhi().set_viewport_vsync(m_viewport.get(), settings.vsync);
        m_applied_vsync = settings.vsync;
    }
}

void GraphicsLayer::pace_frame()
{
    using clock = std::chrono::steady_clock;
    const clock::time_point now = clock::now();
    m_next_frame += m_frame_period;
    if (now >= m_next_frame)
    {
        m_next_frame = now;
        return;
    }
    // OS sleeps overshoot by about a millisecond, so sleep short of the deadline and yield-spin the rest.
    std::this_thread::sleep_until(m_next_frame - std::chrono::milliseconds(1));
    while (clock::now() < m_next_frame)
    {
        std::this_thread::yield();
    }
}

void GraphicsLayer::detach()
{
    m_settings_subscription.reset();
    m_clients.clear();
    if (Renderer::initialised())
    {
        Renderer::set_viewport({});
    }
    m_viewport.reset();
}

void GraphicsLayer::add_client(IFrameClient& client)
{
    m_clients.push_back(&client);
}

void GraphicsLayer::remove_client(IFrameClient& client)
{
    m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), &client), m_clients.end());
}

void GraphicsLayer::set_clear_colour(const Colour& colour)
{
    m_clear = colour;
}

void GraphicsLayer::update(double delta_time)
{
#ifndef OX_DIST
    if (m_reload_key != KeyCode::Unknown && Renderer::initialised() && m_window->input().key_pressed(m_reload_key))
    {
        try
        {
            Renderer::reload_shaders();
            OX_CORE_INFO("GraphicsLayer: shaders reloaded.");
        }
        catch (const Error& error)
        {
            error.log();
            OX_CORE_ERROR("GraphicsLayer: shader reload failed, keeping the running shaders.");
        }
    }
#endif

    NativeWindowHandle handle = m_window->native_handle();
    FrameInfo info{ m_window->input(),
                    { static_cast<float>(handle.width), static_cast<float>(handle.height) },
                    { static_cast<float>(handle.framebuffer_width), static_cast<float>(handle.framebuffer_height) },
                    handle.content_scale,
                    delta_time };
    if (m_viewport)
    {
        const Camera2D camera = Camera2D::screen_space(std::max(info.logical[0], 1.0f), std::max(info.logical[1], 1.0f));
        SceneRenderer& scene = Renderer::scene();
        scene.begin_scene(make_render_view(camera, info));
        run_clients(info);
        try
        {
            scene.end_scene();
        }
        catch (const Error& error)
        {
            error.log();
            OX_CORE_ERROR("GraphicsLayer: dropped every frame client after a scene error.");
            m_clients.clear();
        }
    }
    else
    {
        run_clients(info);
    }

    bool idle = false;
    if (m_viewport)
    {
        Renderer::set_clear_colour(m_clear);
        idle = !Renderer::end_frame();
    }
    m_window->poll_events();
    if (m_window->should_close())
    {
        Application::Get().close();
    }

    if (idle && m_idle_sleep.count() > 0)
    {
        std::this_thread::sleep_for(m_idle_sleep);
        m_next_frame = std::chrono::steady_clock::now();
    }
    else if (m_frame_period.count() > 0)
    {
        pace_frame();
    }
}

void GraphicsLayer::run_clients(const FrameInfo& info)
{
    for (size_t index = 0; index < m_clients.size();)
    {
        try
        {
            m_clients[index]->frame(info);
            ++index;
        }
        catch (const Error& error)
        {
            error.log();
            OX_CORE_ERROR("GraphicsLayer: dropped a frame client after an error.");
            m_clients.erase(m_clients.begin() + static_cast<std::ptrdiff_t>(index));
        }
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
    dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent&)
    {
        if (m_viewport)
        {
            NativeWindowHandle handle = m_window->native_handle();
            Renderer::rhi().resize_viewport(m_viewport.get(), static_cast<uint32_t>(handle.framebuffer_width), static_cast<uint32_t>(handle.framebuffer_height), handle.content_scale);
        }
        return false;
    });
}

} // namespace oryx
