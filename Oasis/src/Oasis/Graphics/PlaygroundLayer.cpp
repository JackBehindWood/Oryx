#include "PlaygroundLayer.h"

#include "Oryx.h"

namespace oasis
{

namespace
{

constexpr uint32_t LINE_COUNT = 12;

oryx::Texture2D make_checker_texture()
{
    uint8_t pixels[4 * 4 * 4];
    for (uint32_t y = 0; y < 4; ++y)
    {
        for (uint32_t x = 0; x < 4; ++x)
        {
            const uint8_t value = ((x + y) % 2 == 0) ? 255 : 40;
            uint8_t* pixel = pixels + (y * 4 + x) * 4;
            pixel[0] = value;
            pixel[1] = value;
            pixel[2] = value;
            pixel[3] = 255;
        }
    }
    return oryx::Renderer::create_texture_2d({ .width = 4, .height = 4, .pixels = pixels, .pixel_bytes = sizeof(pixels), .filter = oryx::RHIFilter::Nearest, .address = oryx::RHIAddressMode::Repeat });
}

oryx::Vec2f on_circle(const oryx::Vec2f& centre, float radius, float angle)
{
    return { centre[0] + radius * oryx::math::cos(angle), centre[1] + radius * oryx::math::sin(angle) };
}

} // namespace

PlaygroundLayer::PlaygroundLayer()
    : Layer("PlaygroundLayer")
{
}

PlaygroundLayer::~PlaygroundLayer() = default;

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

    oryx::IRHI& rhi = oryx::Renderer::rhi();
    OX_INFO("Playground RHI {} on '{}', {} frames in flight", oryx::to_string(rhi.backend()), rhi.capabilities().name, rhi.capabilities().frames_in_flight);

    m_checker = oryx::create_unique<oryx::Texture2D>(make_checker_texture());
}

void PlaygroundLayer::detach()
{
    m_checker.reset();
}

void PlaygroundLayer::update(double delta_time)
{
    m_time += delta_time;
    if (!m_checker || m_size[0] <= 0.0f || m_size[1] <= 0.0f)
    {
        return;
    }

    const float t = static_cast<float>(m_time);
    const float pulse = 0.5f + 0.5f * oryx::math::sin(t * 1.5f);
    oryx::Camera2D camera(2.0f * m_size[0] / m_size[1], 2.0f);
    camera.set_rotation(0.12f * oryx::math::sin(t * 0.5f));

    oryx::Renderer::begin_scene(camera);

    const oryx::Vec2f triangle_centre(-0.55f, 0.4f);
    const float spin = t * 1.2f;
    oryx::Renderer::draw_triangle(on_circle(triangle_centre, 0.35f, spin), on_circle(triangle_centre, 0.35f, spin + 2.094f), on_circle(triangle_centre, 0.35f, spin + 4.189f),
        { 1.0f, 0.2f, 0.2f, 1.0f }, { 0.2f, 1.0f, 0.2f, 1.0f }, { 0.2f, 0.4f, 1.0f, 1.0f });

    const oryx::Vec2f line_origin(-0.55f, -0.45f);
    for (uint32_t i = 0; i < LINE_COUNT; ++i)
    {
        const float angle = -t * 0.8f + static_cast<float>(i) * (6.2832f / static_cast<float>(LINE_COUNT));
        const float shade = static_cast<float>(i) / static_cast<float>(LINE_COUNT);
        oryx::Renderer::draw_line(line_origin, on_circle(line_origin, 0.35f, angle), { 1.0f, shade, 0.3f, 1.0f }, { 0.3f, shade, 1.0f, 1.0f });
    }

    oryx::Renderer::draw_sprite({ 0.5f, 0.45f }, { 0.5f, 0.5f }, *m_checker, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.0f, { 0.0f, 0.0f }, { 2.0f, 2.0f });

    const float rect_top = 0.1f + 0.1f * pulse;
    oryx::Renderer::draw_rect({ 0.5f, (rect_top - 0.05f) * 0.5f }, { 0.5f, rect_top + 0.05f }, { 0.95f, 0.6f, 0.1f, 1.0f });

    oryx::Renderer::draw_circle({ 0.5f, -0.65f }, 0.25f, { 0.3f, 0.9f, 0.8f, 1.0f }, 0.05f + 0.45f * pulse, 0.02f + 0.08f * (1.0f - pulse));

    oryx::Renderer::end_scene();
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
    OX_TRACE("Playground key pressed {}", static_cast<int32_t>(event.key()));
    return false;
}

bool PlaygroundLayer::on_key_released(oryx::KeyReleasedEvent& event)
{
    OX_TRACE("Playground key released {}", static_cast<int32_t>(event.key()));
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
    OX_TRACE("Playground mouse button pressed {}", static_cast<int32_t>(event.button()));
    return false;
}

bool PlaygroundLayer::on_mouse_button_released(oryx::MouseButtonReleasedEvent& event)
{
    OX_TRACE("Playground mouse button released {}", static_cast<int32_t>(event.button()));
    return false;
}

bool PlaygroundLayer::on_mouse_scrolled(oryx::MouseScrolledEvent& event)
{
    OX_TRACE("Playground mouse scrolled {}, {}", event.dx(), event.dy());
    return false;
}

} // namespace oasis
