#include "PlaygroundLayer.h"

#include "Oryx.h"

namespace oasis
{

namespace
{

constexpr uint32_t LINE_COUNT = 12;
constexpr double FPS_WINDOW_SECONDS = 0.5;
constexpr float OVERLAY_PIXEL_HEIGHT = 16.0f;
constexpr float OVERLAY_MARGIN = 8.0f;
constexpr const char* FONT_PATH = "fonts/PressStart2P-Regular.ttf";
constexpr const char* IMAGE_PATH = "images/checker.png";

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

    m_font_asset = oryx::Assets::load<oryx::FontAsset>(FONT_PATH);
    m_image_asset = oryx::Assets::load<oryx::ImageAsset>(IMAGE_PATH);
    m_font = oryx::create_unique<oryx::Font>(oryx::Font::create(oryx::make_font_source(oryx::Assets::manager(), m_font_asset)));
}

void PlaygroundLayer::detach()
{
    m_font.reset();
    m_gpu_assets.clear();
    oryx::Assets::release(m_font_asset);
    oryx::Assets::release(m_image_asset);
}

void PlaygroundLayer::update(double delta_time)
{
    m_time += delta_time;
    track_frame_time(delta_time);
    if (m_size[0] <= 0.0f || m_size[1] <= 0.0f)
    {
        return;
    }

    const bool reload_down = oryx::Input::key_pressed(oryx::KeyCode::R);
    if (reload_down && !m_reload_down)
    {
        OX_INFO("Playground reloading '{}' and '{}'", FONT_PATH, IMAGE_PATH);
        oryx::Assets::reload(m_font_asset);
        oryx::Assets::reload(m_image_asset);
    }
    m_reload_down = reload_down;

    draw_world();
    draw_overlay();
}

void PlaygroundLayer::track_frame_time(double delta_time)
{
    m_fps_time += delta_time;
    ++m_fps_frames;
    if (m_fps_time >= FPS_WINDOW_SECONDS)
    {
        m_fps = static_cast<float>(static_cast<double>(m_fps_frames) / m_fps_time);
        m_frame_ms = static_cast<float>(1000.0 * m_fps_time / static_cast<double>(m_fps_frames));
        m_fps_time = 0.0;
        m_fps_frames = 0;
    }
}

void PlaygroundLayer::draw_world()
{
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

    const oryx::Texture2D& image = m_gpu_assets.get(oryx::Renderer::rhi(), oryx::Assets::manager(), m_image_asset);
    oryx::Renderer::draw_sprite({ 0.5f, 0.45f }, { 0.5f, 0.5f }, image);

    const float rect_top = 0.1f + 0.1f * pulse;
    oryx::Renderer::draw_rect({ 0.5f, (rect_top - 0.05f) * 0.5f }, { 0.5f, rect_top + 0.05f }, { 0.95f, 0.6f, 0.1f, 1.0f });

    oryx::Renderer::draw_circle({ 0.5f, -0.65f }, 0.25f, { 0.3f, 0.9f, 0.8f, 1.0f }, 0.05f + 0.45f * pulse, 0.02f + 0.08f * (1.0f - pulse));

    oryx::Renderer::end_scene();
}

void PlaygroundLayer::draw_overlay()
{
    if (!m_font->ready())
    {
        return;
    }

    // The overlay's own draws are recorded after this read, so they show up in next frame's numbers.
    const oryx::BatchStats stats = oryx::Renderer::batch_stats();
    char lines[3][96];
    std::snprintf(lines[0], sizeof(lines[0]), "FPS %.1f (%.2f ms)", m_fps, m_frame_ms);
    std::snprintf(lines[1], sizeof(lines[1]), "draws %u tris %u verts %u slots %u", stats.draws, stats.triangles, stats.vertices, stats.texture_slots_used);
    std::snprintf(lines[2], sizeof(lines[2]), "font rev %u image rev %u", oryx::Assets::revision(m_font_asset), oryx::Assets::revision(m_image_asset));

    oryx::Camera2D camera(m_size[0], m_size[1]);
    camera.set_position({ m_size[0] * 0.5f, m_size[1] * 0.5f });
    oryx::TextStyle style;
    style.pixel_height = OVERLAY_PIXEL_HEIGHT;
    const float line_height = m_font->line_height(OVERLAY_PIXEL_HEIGHT);
    float baseline = m_size[1] - OVERLAY_MARGIN - m_font->ascent(OVERLAY_PIXEL_HEIGHT);

    oryx::Renderer::begin_scene(camera);
    for (const char* line : lines)
    {
        oryx::Renderer::draw_text({ OVERLAY_MARGIN, baseline }, line, *m_font, style);
        baseline -= line_height;
    }
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
