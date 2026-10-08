#include "oxpch.h"
#include "Oryx/Renderer/GraphicsLayer.h"

#include "Oryx/Core/Application.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Core/Timer.h"
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
    m_idle_sleep_ms = settings.idle_sleep_ms;
    m_pacer.set_rate(settings.max_fps);
    m_reload_key = key_from_name(settings.reload_key);
    if (m_viewport && Renderer::initialised() && settings.vsync != m_applied_vsync)
    {
        Renderer::rhi().set_viewport_vsync(m_viewport.get(), settings.vsync);
        m_applied_vsync = settings.vsync;
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

void GraphicsLayer::add_client(IFrameClient& client, const ClientDesc& desc)
{
    m_clients.push_back({ &client, desc });
}

void GraphicsLayer::remove_client(IFrameClient& client)
{
    m_clients.erase(std::remove_if(m_clients.begin(), m_clients.end(), [&client](const Client& entry) { return entry.client == &client; }), m_clients.end());
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
    Timer timer;
    timer.start();
    FrameCpuTimes cpu;
    const auto lap_ms = [&timer] { return static_cast<float>(timer.tick() * 1000.0); };
    if (m_viewport)
    {
        const Camera2D camera = Camera2D::screen_space(std::max(info.logical[0], 1.0f), std::max(info.logical[1], 1.0f));
        SceneRenderer& scene = Renderer::scene();
        scene.begin_scene(make_render_view(camera, info));
        run_phases(info, &scene);
        cpu.clients_ms = lap_ms();
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
        cpu.scene_ms = lap_ms();
    }
    else
    {
        run_phases(info, nullptr);
    }

    bool idle = false;
    if (m_viewport)
    {
        Renderer::set_clear_colour(m_clear);
        idle = !Renderer::end_frame();
        cpu.record_ms = lap_ms();
        Renderer::set_frame_cpu_times(cpu);
    }
    m_window->poll_events();
    if (m_window->should_close())
    {
        Application::Get().close();
    }

    if (idle && m_idle_sleep_ms > 0)
    {
        FramePacer::sleep_ms(m_idle_sleep_ms);
        m_pacer.reset();
    }
    else if (m_pacer.capped())
    {
        m_pacer.wait();
    }
}

namespace
{

void call_client(IFrameClient& client, const FrameInfo& info, SceneRenderer* scene, const SubmitContext& context)
{
    if (scene == nullptr)
    {
        client.frame(info);
        return;
    }
    SubmitScope submit(*scene, context);
    client.frame(info);
}

} // namespace

void GraphicsLayer::run_phases(const FrameInfo& info, SceneRenderer* scene)
{
    m_router.begin_frame(info.input, info.logical);
    const FrameInfo interface_info{ m_router.interface_input(), info.logical, info.framebuffer, info.scale, info.delta_time };
    run_clients(FramePhase::Interface, interface_info, scene);
    m_router.begin_world();
    run_clients(FramePhase::World, info, scene);
}

void GraphicsLayer::run_clients(FramePhase phase, const FrameInfo& info, SceneRenderer* scene)
{
    for (size_t index = 0; index < m_clients.size();)
    {
        const Client entry = m_clients[index];
        if (entry.desc.phase != phase)
        {
            ++index;
            continue;
        }
        try
        {
            if (phase == FramePhase::Interface)
            {
                call_client(*entry.client, info, scene, { {}, false, k_layer_interface });
            }
            else
            {
                ViewRegion region;
                if (m_router.has_view(entry.desc.view))
                {
                    region = m_router.view(entry.desc.view);
                }
                const bool whole = math::approx_equal(region.size[0], info.logical[0]) && math::approx_equal(region.size[1], info.logical[1]);
                const Vec2f framebuffer = whole ? info.framebuffer : Vec2f(region.size[0] * info.scale, region.size[1] * info.scale);
                const FrameInfo view_info{ m_router.world_input(entry.desc.view), region.size, framebuffer, info.scale, info.delta_time };
                call_client(*entry.client, view_info, scene, { region, true, k_layer_world });
            }
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
