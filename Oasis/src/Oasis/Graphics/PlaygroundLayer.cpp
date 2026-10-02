#include "PlaygroundLayer.h"

#include "Oryx.h"

namespace oasis
{

oryx::Colour playground_clear_colour(double time, oryx::Vec2f cursor, oryx::Vec2f size)
{
    float u = size[0] > 0.0f ? cursor[0] / size[0] : 0.0f;
    float v = size[1] > 0.0f ? cursor[1] / size[1] : 0.0f;
    float t = static_cast<float>(time);

    oryx::Colour colour;
    colour.r = oryx::math::saturate(0.5f + 0.5f * oryx::math::sin(t * 0.7f) * (0.5f + 0.5f * u));
    colour.g = oryx::math::saturate(0.5f + 0.5f * oryx::math::sin(t * 1.1f + 2.0f) * (0.5f + 0.5f * v));
    colour.b = oryx::math::saturate(0.5f + 0.5f * oryx::math::sin(t * 1.7f + 4.0f));
    colour.a = 1.0f;
    return colour;
}

PlaygroundLayer::PlaygroundLayer(oryx::RHIBackend backend)
    : Layer("PlaygroundLayer")
    , m_backend(backend)
{
}

void PlaygroundLayer::attach()
{
    m_window = oryx::Application::Get().window();
    if (!m_window)
    {
        throw oryx::Error("PlaygroundLayer requires an active window");
    }

    oryx::NativeWindowHandle handle = m_window->native_handle();
    m_size = oryx::Vec2f(static_cast<float>(handle.width), static_cast<float>(handle.height));
    OX_INFO("Playground window {}x{}, content scale {}", handle.width, handle.height, handle.content_scale);

    oryx::Renderer::init({ m_backend });
    oryx::IRHI& rhi = oryx::Renderer::rhi();
    OX_INFO("Playground RHI {} on '{}', {} frames in flight", oryx::to_string(m_backend), rhi.capabilities().name, rhi.capabilities().frames_in_flight);
   
    // Created and dropped at once to exercise deferred retirement.
    const std::array<uint8_t, 64> pixels = {};
    rhi.create_buffer({ .size = 64, .initial_data = pixels.data(), .initial_data_size = 64 });
    rhi.create_texture({ .width = 4, .height = 4, .initial_data = pixels.data(), .initial_data_size = 64 });
    rhi.create_sampler({});
}

void PlaygroundLayer::update(double delta_time)
{
    m_time += delta_time;

    oryx::Vec2f cursor;
    oryx::Input::cursor_position(cursor);
    m_clear = playground_clear_colour(m_time, cursor, m_size);
    oryx::Renderer::clear(m_clear);
}

void PlaygroundLayer::event(oryx::Event& event)
{
    oryx::EventDispatcher dispatcher(event);
    dispatcher.dispatch<oryx::WindowResizeEvent>(OX_BIND_EVENT_FN(on_window_resize));
    dispatcher.dispatch<oryx::WindowFocusEvent>(OX_BIND_EVENT_FN(on_window_focus));
    dispatcher.dispatch<oryx::WindowCloseEvent>(OX_BIND_EVENT_FN(on_window_close));
    dispatcher.dispatch<oryx::ApplicationCloseEvent>(OX_BIND_EVENT_FN(on_application_close));
    dispatcher.dispatch<oryx::KeyPressedEvent>(OX_BIND_EVENT_FN(on_key_pressed));
    dispatcher.dispatch<oryx::KeyReleasedEvent>(OX_BIND_EVENT_FN(on_key_released));
    dispatcher.dispatch<oryx::MouseMovedEvent>(OX_BIND_EVENT_FN(on_mouse_moved));
    dispatcher.dispatch<oryx::MouseButtonPressedEvent>(OX_BIND_EVENT_FN(on_mouse_button_pressed));
    dispatcher.dispatch<oryx::MouseButtonReleasedEvent>(OX_BIND_EVENT_FN(on_mouse_button_released));
    dispatcher.dispatch<oryx::MouseScrolledEvent>(OX_BIND_EVENT_FN(on_mouse_scrolled));
}

bool PlaygroundLayer::on_window_resize(oryx::WindowResizeEvent& event)
{
    m_size = oryx::Vec2f(static_cast<float>(event.width()), static_cast<float>(event.height()));
    OX_INFO("Playground resize {}x{}", event.width(), event.height());
    return false;
}

bool PlaygroundLayer::on_window_focus(oryx::WindowFocusEvent& event)
{
    OX_INFO("Playground window {}", event.focused() ? "focused" : "unfocused");
    return false;
}

bool PlaygroundLayer::on_window_close(oryx::WindowCloseEvent&)
{
    OX_INFO("Playground window close requested");
    return false;
}

bool PlaygroundLayer::on_application_close(oryx::ApplicationCloseEvent& event)
{
    OX_INFO("Playground application closing (exit code {})", event.exit_code());
    return false;
}

bool PlaygroundLayer::on_key_pressed(oryx::KeyPressedEvent& event)
{
    OX_INFO("Playground key pressed {}", static_cast<int32_t>(event.key()));
    return false;
}

bool PlaygroundLayer::on_key_released(oryx::KeyReleasedEvent& event)
{
    OX_INFO("Playground key released {}", static_cast<int32_t>(event.key()));
    return false;
}

bool PlaygroundLayer::on_mouse_moved(oryx::MouseMovedEvent& event)
{
    if (oryx::Input::mouse_down(oryx::MouseCode::Left))
    {
        OX_TRACE("Playground mouse moved {}, {}", event.x(), event.y());
    }
    return false;
}

bool PlaygroundLayer::on_mouse_button_pressed(oryx::MouseButtonPressedEvent& event)
{
    OX_INFO("Playground mouse button pressed {}", static_cast<int32_t>(event.button()));
    return false;
}

bool PlaygroundLayer::on_mouse_button_released(oryx::MouseButtonReleasedEvent& event)
{
    OX_INFO("Playground mouse button released {}", static_cast<int32_t>(event.button()));
    return false;
}

bool PlaygroundLayer::on_mouse_scrolled(oryx::MouseScrolledEvent& event)
{
    OX_INFO("Playground mouse scrolled {}, {}", event.dx(), event.dy());
    return false;
}

} // namespace oasis
