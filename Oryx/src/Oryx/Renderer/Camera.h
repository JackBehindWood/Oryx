#pragma once

#include "Oryx/Math/Math.h"

namespace oryx
{

// A projection and a view matrix; the base of every camera, so a renderer takes a `const Camera&` for 2D and 3D alike.
class Camera
{
public:
    Camera() = default;
    Camera(const Mat4f& projection, const Mat4f& view)
        : m_projection(projection)
        , m_view(view)
    {
    }

    [[nodiscard]] const Mat4f& projection() const { return m_projection; }
    [[nodiscard]] const Mat4f& view() const { return m_view; }
    [[nodiscard]] Mat4f view_projection() const { return m_projection * m_view; }

    // Column-major view_projection, the layout the built-in shaders' frame constants read.
    void to_gpu(float (&out)[16]) const;

protected:
    Mat4f m_projection = Mat4f::identity();
    Mat4f m_view = Mat4f::identity();
};

// Orthographic, y up, centred on `position`; the visible world is viewport size / zoom. Screen space is pixels with the origin top left, y down.
class Camera2D : public Camera
{
public:
    Camera2D(float viewport_width, float viewport_height);

    void set_position(const Vec2f& position);
    void set_rotation(float radians);
    void set_zoom(float zoom);
    void set_viewport(float width, float height);

    [[nodiscard]] const Vec2f& position() const { return m_position; }
    [[nodiscard]] float rotation() const { return m_rotation; }
    [[nodiscard]] float zoom() const { return m_zoom; }
    [[nodiscard]] float viewport_width() const { return m_viewport_width; }
    [[nodiscard]] float viewport_height() const { return m_viewport_height; }

    [[nodiscard]] Vec2f screen_to_world(const Vec2f& screen) const;
    [[nodiscard]] Vec2f world_to_screen(const Vec2f& world) const;

private:
    void rebuild();

    Vec2f m_position{ 0.0f, 0.0f };
    float m_rotation = 0.0f;
    float m_zoom = 1.0f;
    float m_viewport_width;
    float m_viewport_height;
};

} // namespace oryx
