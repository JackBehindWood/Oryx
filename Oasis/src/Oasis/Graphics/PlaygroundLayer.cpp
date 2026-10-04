#include "PlaygroundLayer.h"

#include "Oryx.h"

namespace oasis
{

namespace
{

struct SolidVertex
{
    float position[3];
    float colour[4];
};

struct QuadVertex
{
    float position[3];
    float colour[4];
    float uv[2];
    float tex_index;
};

struct CircleVertex
{
    float position[3];
    float colour[4];
    float local_position[2];
    float thickness;
    float fade;
};

static_assert(sizeof(SolidVertex) == 28 && sizeof(QuadVertex) == 40 && sizeof(CircleVertex) == 44, "vertex structs must match the built-in layouts");

constexpr uint32_t LINE_COUNT = 12;
constexpr uint32_t TEXTURE_CHECKER = 0;
// The renderer fills every texture slot the item leaves unset with its white default.
constexpr uint32_t TEXTURE_WHITE = 1;

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

// Column-major, as MSL expects: aspect-correct, with a slow wobble so the constants visibly change per frame.
void playground_view_projection(float aspect, float time, float (&matrix)[16])
{
    const float angle = 0.12f * oryx::math::sin(time * 0.5f);
    const float c = oryx::math::cos(angle);
    const float s = oryx::math::sin(angle);
    const float sx = aspect > 1.0f ? 1.0f / aspect : 1.0f;
    const float sy = aspect > 1.0f ? 1.0f : aspect;
    const float values[16] = { c * sx, s * sy, 0.0f, 0.0f, -s * sx, c * sy, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    std::memcpy(matrix, values, sizeof(values));
}

} // namespace

struct PlaygroundScene
{
    PlaygroundScene()
        : triangle_pipeline(oryx::builtin_solid_triangles())
        , line_pipeline(oryx::builtin_solid_lines())
        , quad_pipeline(oryx::builtin_quad())
        , circle_pipeline(oryx::builtin_circle())
        , triangle_vertices(oryx::Renderer::create_vertex_buffer(oryx::solid_vertex_layout(), 3, oryx::BufferMode::Dynamic))
        , line_vertices(oryx::Renderer::create_vertex_buffer(oryx::solid_vertex_layout(), LINE_COUNT * 2, oryx::BufferMode::Dynamic))
        , quad_vertices(oryx::Renderer::create_vertex_buffer(oryx::quad_vertex_layout(), 8, oryx::BufferMode::Dynamic))
        , circle_vertices(oryx::Renderer::create_vertex_buffer(oryx::circle_vertex_layout(), 4, oryx::BufferMode::Dynamic))
        , indices(oryx::Renderer::create_index_buffer(oryx::IndexType::U16, 12, oryx::BufferMode::Static))
        , checker(make_checker_texture())
    {
        const uint16_t quad_indices[12] = { 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4 };
        indices.set_data(0, quad_indices);
    }

    oryx::GraphicsPipelineHandle triangle_pipeline;
    oryx::GraphicsPipelineHandle line_pipeline;
    oryx::GraphicsPipelineHandle quad_pipeline;
    oryx::GraphicsPipelineHandle circle_pipeline;
    oryx::VertexBuffer triangle_vertices;
    oryx::VertexBuffer line_vertices;
    oryx::VertexBuffer quad_vertices;
    oryx::VertexBuffer circle_vertices;
    oryx::IndexBuffer indices;
    oryx::Texture2D checker;
};

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

    m_scene = oryx::create_unique<PlaygroundScene>();
}

void PlaygroundLayer::detach()
{
    m_scene.reset();
}

void PlaygroundLayer::update(double delta_time)
{
    m_time += delta_time;
    if (!m_scene)
    {
        return;
    }

    PlaygroundScene& scene = *m_scene;
    const float t = static_cast<float>(m_time);
    const uint32_t slot = oryx::Renderer::frame_slot();
    const float aspect = m_size[1] > 0.0f ? m_size[0] / m_size[1] : 1.0f;
    float view_projection[16];
    playground_view_projection(aspect, t, view_projection);

    const float spin = t * 1.2f;
    const SolidVertex triangle[3] = {
        { { -0.55f + 0.35f * oryx::math::cos(spin), 0.4f + 0.35f * oryx::math::sin(spin), 0.0f }, { 1.0f, 0.2f, 0.2f, 1.0f } },
        { { -0.55f + 0.35f * oryx::math::cos(spin + 2.094f), 0.4f + 0.35f * oryx::math::sin(spin + 2.094f), 0.0f }, { 0.2f, 1.0f, 0.2f, 1.0f } },
        { { -0.55f + 0.35f * oryx::math::cos(spin + 4.189f), 0.4f + 0.35f * oryx::math::sin(spin + 4.189f), 0.0f }, { 0.2f, 0.4f, 1.0f, 1.0f } },
    };
    scene.triangle_vertices.set_data(slot, triangle);

    SolidVertex lines[LINE_COUNT * 2];
    for (uint32_t i = 0; i < LINE_COUNT; ++i)
    {
        const float angle = -t * 0.8f + static_cast<float>(i) * (6.2832f / static_cast<float>(LINE_COUNT));
        const float shade = static_cast<float>(i) / static_cast<float>(LINE_COUNT);
        lines[i * 2] = { { -0.55f, -0.45f, 0.0f }, { 1.0f, shade, 0.3f, 1.0f } };
        lines[i * 2 + 1] = { { -0.55f + 0.35f * oryx::math::cos(angle), -0.45f + 0.35f * oryx::math::sin(angle), 0.0f }, { 0.3f, shade, 1.0f, 1.0f } };
    }
    scene.line_vertices.set_data(slot, lines);

    const float tex_checker = static_cast<float>(TEXTURE_CHECKER);
    const float tex_white = static_cast<float>(TEXTURE_WHITE);
    const float pulse = 0.5f + 0.5f * oryx::math::sin(t * 1.5f);
    const QuadVertex quads[8] = {
        { { 0.25f, 0.2f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, tex_checker },
        { { 0.75f, 0.2f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 2.0f, 0.0f }, tex_checker },
        { { 0.75f, 0.7f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 2.0f, 2.0f }, tex_checker },
        { { 0.25f, 0.7f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 2.0f }, tex_checker },
        { { 0.25f, -0.05f, 0.0f }, { 0.95f, 0.6f, 0.1f, 1.0f }, { 0.0f, 0.0f }, tex_white },
        { { 0.75f, -0.05f, 0.0f }, { 0.95f, 0.6f, 0.1f, 1.0f }, { 1.0f, 0.0f }, tex_white },
        { { 0.75f, 0.1f + 0.1f * pulse, 0.0f }, { 0.95f, 0.6f, 0.1f, 1.0f }, { 1.0f, 1.0f }, tex_white },
        { { 0.25f, 0.1f + 0.1f * pulse, 0.0f }, { 0.95f, 0.6f, 0.1f, 1.0f }, { 0.0f, 1.0f }, tex_white },
    };
    scene.quad_vertices.set_data(slot, quads);

    const float thickness = 0.05f + 0.45f * pulse;
    const float fade = 0.02f + 0.08f * (1.0f - pulse);
    const CircleVertex circle[4] = {
        { { 0.25f, -0.9f, 0.0f }, { 0.3f, 0.9f, 0.8f, 1.0f }, { -1.0f, -1.0f }, thickness, fade },
        { { 0.75f, -0.9f, 0.0f }, { 0.3f, 0.9f, 0.8f, 1.0f }, { 1.0f, -1.0f }, thickness, fade },
        { { 0.75f, -0.4f, 0.0f }, { 0.3f, 0.9f, 0.8f, 1.0f }, { 1.0f, 1.0f }, thickness, fade },
        { { 0.25f, -0.4f, 0.0f }, { 0.3f, 0.9f, 0.8f, 1.0f }, { -1.0f, 1.0f }, thickness, fade },
    };
    scene.circle_vertices.set_data(slot, circle);

    oryx::DrawItem triangle_item;
    triangle_item.pipeline = scene.triangle_pipeline;
    oryx::draw_item_set_vertices(triangle_item, scene.triangle_vertices, slot, 3);
    oryx::draw_item_set_constants(triangle_item, view_projection);
    oryx::Renderer::submit(triangle_item);

    oryx::DrawItem line_item;
    line_item.pipeline = scene.line_pipeline;
    oryx::draw_item_set_vertices(line_item, scene.line_vertices, slot, LINE_COUNT * 2);
    oryx::draw_item_set_constants(line_item, view_projection);
    oryx::Renderer::submit(line_item);

    oryx::DrawItem quad_item;
    quad_item.pipeline = scene.quad_pipeline;
    oryx::draw_item_set_vertices(quad_item, scene.quad_vertices, slot, 8);
    oryx::draw_item_set_indices(quad_item, scene.indices, slot, 12);
    oryx::draw_item_set_constants(quad_item, view_projection);
    oryx::draw_item_add_texture(quad_item, scene.checker.texture());
    quad_item.sampler = scene.checker.sampler();
    oryx::Renderer::submit(quad_item);

    oryx::DrawItem circle_item;
    circle_item.pipeline = scene.circle_pipeline;
    oryx::draw_item_set_vertices(circle_item, scene.circle_vertices, slot, 4);
    oryx::draw_item_set_indices(circle_item, scene.indices, slot, 6);
    oryx::draw_item_set_constants(circle_item, view_projection);
    oryx::Renderer::submit(circle_item);
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
