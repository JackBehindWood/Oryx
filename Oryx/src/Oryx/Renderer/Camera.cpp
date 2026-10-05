#include "oxpch.h"
#include "Oryx/Renderer/Camera.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

void Camera::to_gpu(float (&out)[16]) const
{
    to_column_major(view_projection(), out);
}

Camera2D::Camera2D(float viewport_width, float viewport_height)
    : m_viewport_width(viewport_width)
    , m_viewport_height(viewport_height)
{
    if (viewport_width <= 0.0f || viewport_height <= 0.0f)
    {
        throw Error("Camera2D viewport must be positive");
    }
    rebuild();
}

void Camera2D::set_position(const Vec2f& position)
{
    m_position = position;
    rebuild();
}

void Camera2D::set_rotation(float radians)
{
    m_rotation = radians;
    rebuild();
}

void Camera2D::set_zoom(float zoom)
{
    if (zoom <= 0.0f)
    {
        throw Error("Camera2D zoom must be positive");
    }
    m_zoom = zoom;
    rebuild();
}

void Camera2D::set_viewport(float width, float height)
{
    if (width <= 0.0f || height <= 0.0f)
    {
        throw Error("Camera2D viewport must be positive");
    }
    m_viewport_width = width;
    m_viewport_height = height;
    rebuild();
}

Vec2f Camera2D::screen_to_world(const Vec2f& screen) const
{
    const Vec4f ndc(2.0f * screen[0] / m_viewport_width - 1.0f, 1.0f - 2.0f * screen[1] / m_viewport_height, 0.0f, 1.0f);
    const Vec4f world = inverse(view_projection()) * ndc;
    return Vec2f(world[0], world[1]);
}

Vec2f Camera2D::world_to_screen(const Vec2f& world) const
{
    const Vec4f clip = view_projection() * Vec4f(world[0], world[1], 0.0f, 1.0f);
    return Vec2f((clip[0] + 1.0f) * 0.5f * m_viewport_width, (1.0f - clip[1]) * 0.5f * m_viewport_height);
}

void Camera2D::rebuild()
{
    const float half_width = m_viewport_width / (2.0f * m_zoom);
    const float half_height = m_viewport_height / (2.0f * m_zoom);
    m_projection = orthographic(-half_width, half_width, -half_height, half_height, -1.0f, 1.0f);
    m_view = oryx::rotation(Vec3f(0.0f, 0.0f, 1.0f), -m_rotation) * translation(Vec3f(-m_position[0], -m_position[1], 0.0f));
}

} // namespace oryx
