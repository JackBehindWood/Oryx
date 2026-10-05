#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

namespace
{

void check_near(const Mat4f& a, const Mat4f& b, float epsilon = 1e-4f)
{
    for (size_t row = 0; row < 4; ++row)
    {
        for (size_t col = 0; col < 4; ++col)
        {
            CHECK(a.at(row, col) == doctest::Approx(b.at(row, col)).epsilon(epsilon));
        }
    }
}

void check_near(const Vec4f& v, float x, float y, float z, float w = 1.0f)
{
    CHECK(v[0] == doctest::Approx(x).epsilon(1e-4));
    CHECK(v[1] == doctest::Approx(y).epsilon(1e-4));
    CHECK(v[2] == doctest::Approx(z).epsilon(1e-4));
    CHECK(v[3] == doctest::Approx(w).epsilon(1e-4));
}

} // namespace

TEST_CASE("Mat4f translation, scale and rotation transform points")
{
    check_near(translation(Vec3f(1.0f, 2.0f, 3.0f)) * Vec4f(1.0f, 1.0f, 1.0f, 1.0f), 2.0f, 3.0f, 4.0f);
    check_near(translation(Vec3f(1.0f, 2.0f, 3.0f)) * Vec4f(1.0f, 1.0f, 1.0f, 0.0f), 1.0f, 1.0f, 1.0f, 0.0f);
    check_near(scale(Vec3f(2.0f, 3.0f, 4.0f)) * Vec4f(1.0f, 1.0f, 1.0f, 1.0f), 2.0f, 3.0f, 4.0f);

    const float quarter = math::HALF_PI<float>;
    check_near(rotation(Vec3f(0.0f, 0.0f, 1.0f), quarter) * Vec4f(1.0f, 0.0f, 0.0f, 1.0f), 0.0f, 1.0f, 0.0f);
    check_near(rotation(Vec3f(1.0f, 0.0f, 0.0f), quarter) * Vec4f(0.0f, 1.0f, 0.0f, 1.0f), 0.0f, 0.0f, 1.0f);
    check_near(rotation(Vec3f(0.0f, 1.0f, 0.0f), quarter) * Vec4f(0.0f, 0.0f, 1.0f, 1.0f), 1.0f, 0.0f, 0.0f);
    check_near(rotation(Vec3f(0.0f, 0.0f, 5.0f), quarter), rotation(Vec3f(0.0f, 0.0f, 1.0f), quarter));
}

TEST_CASE("Mat4f rotation about an arbitrary axis preserves the axis and is orthonormal")
{
    const Vec3f axis(1.0f, 2.0f, 3.0f);
    const Mat4f r = rotation(axis, 0.7f);
    const Vec3f unit = axis.normalized();
    check_near(r * Vec4f(unit[0], unit[1], unit[2], 1.0f), unit[0], unit[1], unit[2]);
    check_near(r * transpose(r), Mat4f::identity());
}

TEST_CASE("Mat4f inverse round-trips and determinant matches")
{
    const Mat4f m = translation(Vec3f(3.0f, -2.0f, 5.0f)) * rotation(Vec3f(1.0f, 1.0f, 0.0f), 0.9f) * scale(Vec3f(2.0f, 3.0f, 0.5f));
    CHECK(determinant(m) == doctest::Approx(3.0f).epsilon(1e-4));
    check_near(m * inverse(m), Mat4f::identity());
    check_near(m.inverse() * m, Mat4f::identity());
    check_near(inverse(Mat4f::identity()), Mat4f::identity());
}

TEST_CASE("Mat4f orthographic maps the box to clip space with depth in [0, 1]")
{
    const Mat4f ortho = orthographic(-4.0f, 4.0f, -2.0f, 2.0f, 1.0f, 11.0f);
    check_near(ortho * Vec4f(-4.0f, -2.0f, 1.0f, 1.0f), -1.0f, -1.0f, 0.0f);
    check_near(ortho * Vec4f(4.0f, 2.0f, 11.0f, 1.0f), 1.0f, 1.0f, 1.0f);

    const Mat4f reverse = orthographic(-4.0f, 4.0f, -2.0f, 2.0f, 1.0f, 11.0f, DepthConvention::ReverseZ);
    check_near(reverse * Vec4f(0.0f, 0.0f, 1.0f, 1.0f), 0.0f, 0.0f, 1.0f);
    check_near(reverse * Vec4f(0.0f, 0.0f, 11.0f, 1.0f), 0.0f, 0.0f, 0.0f);
}

TEST_CASE("Mat4f perspective maps near and far to depth 0 and 1 (or 1 and 0 reversed)")
{
    const Mat4f projection = perspective(math::HALF_PI<float>, 2.0f, 0.5f, 50.0f);
    Vec4f near_point = projection * Vec4f(0.0f, 0.0f, 0.5f, 1.0f);
    Vec4f far_point = projection * Vec4f(0.0f, 0.0f, 50.0f, 1.0f);
    CHECK(near_point[2] / near_point[3] == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(far_point[2] / far_point[3] == doctest::Approx(1.0f).epsilon(1e-4));

    Vec4f edge = projection * Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    CHECK(edge[0] / edge[3] == doctest::Approx(0.5f).epsilon(1e-4));
    CHECK(edge[1] / edge[3] == doctest::Approx(1.0f).epsilon(1e-4));

    const Mat4f reverse = perspective(math::HALF_PI<float>, 2.0f, 0.5f, 50.0f, DepthConvention::ReverseZ);
    near_point = reverse * Vec4f(0.0f, 0.0f, 0.5f, 1.0f);
    far_point = reverse * Vec4f(0.0f, 0.0f, 50.0f, 1.0f);
    CHECK(near_point[2] / near_point[3] == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(far_point[2] / far_point[3] == doctest::Approx(0.0f).epsilon(1e-4));
}

TEST_CASE("Mat4f look_at is left-handed: the eye moves to the origin looking down +Z")
{
    const Mat4f view = look_at(Vec3f(0.0f, 0.0f, -5.0f), Vec3f(0.0f, 0.0f, 0.0f), Vec3f(0.0f, 1.0f, 0.0f));
    check_near(view * Vec4f(0.0f, 0.0f, -5.0f, 1.0f), 0.0f, 0.0f, 0.0f);
    check_near(view * Vec4f(0.0f, 0.0f, 0.0f, 1.0f), 0.0f, 0.0f, 5.0f);
    check_near(view * Vec4f(1.0f, 2.0f, -5.0f, 1.0f), 1.0f, 2.0f, 0.0f);

    const Mat4f side = look_at(Vec3f(5.0f, 0.0f, 0.0f), Vec3f(0.0f, 0.0f, 0.0f), Vec3f(0.0f, 1.0f, 0.0f));
    check_near(side * Vec4f(0.0f, 0.0f, 0.0f, 1.0f), 0.0f, 0.0f, 5.0f);
    check_near(side * Vec4f(0.0f, 0.0f, 1.0f, 1.0f), 1.0f, 0.0f, 5.0f);
}

TEST_CASE("to_column_major packs columns contiguously")
{
    Mat4f m = Mat4f::identity();
    m.at(0, 3) = 7.0f;
    m.at(1, 0) = 5.0f;
    float out[16] = {};
    to_column_major(m, out);
    CHECK(out[12] == 7.0f);
    CHECK(out[1] == 5.0f);
    CHECK(out[0] == 1.0f);
    CHECK(out[15] == 1.0f);

    float gpu[16] = {};
    Camera(m, Mat4f::identity()).to_gpu(gpu);
    CHECK(std::memcmp(gpu, out, sizeof(out)) == 0);
}

TEST_CASE("Camera combines projection and view")
{
    const Mat4f projection = orthographic(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    const Mat4f view = translation(Vec3f(1.0f, 0.0f, 0.0f));
    const Camera camera(projection, view);
    check_near(camera.view_projection(), projection * view);
    check_near(Camera().view_projection(), Mat4f::identity());
}

TEST_CASE("Camera2D shows viewport / zoom world units centred on its position")
{
    Camera2D camera(800.0f, 600.0f);
    check_near(camera.view_projection() * Vec4f(400.0f, 300.0f, 0.0f, 1.0f), 1.0f, 1.0f, 0.5f);
    check_near(camera.view_projection() * Vec4f(0.0f, 0.0f, 0.0f, 1.0f), 0.0f, 0.0f, 0.5f);

    camera.set_zoom(2.0f);
    check_near(camera.view_projection() * Vec4f(200.0f, 150.0f, 0.0f, 1.0f), 1.0f, 1.0f, 0.5f);

    camera.set_position(Vec2f(10.0f, 20.0f));
    check_near(camera.view_projection() * Vec4f(10.0f, 20.0f, 0.0f, 1.0f), 0.0f, 0.0f, 0.5f);
}

TEST_CASE("Camera2D rotation turns the view")
{
    Camera2D camera(200.0f, 100.0f);
    camera.set_rotation(math::HALF_PI<float>);
    check_near(camera.view_projection() * Vec4f(0.0f, 50.0f, 0.0f, 1.0f), 0.5f, 0.0f, 0.5f);
}

TEST_CASE("Camera2D screen_to_world and world_to_screen are inverses with y flipped")
{
    Camera2D camera(800.0f, 600.0f);
    CHECK(camera.screen_to_world(Vec2f(400.0f, 300.0f))[0] == doctest::Approx(0.0f).epsilon(1e-4));
    Vec2f top_left = camera.screen_to_world(Vec2f(0.0f, 0.0f));
    CHECK(top_left[0] == doctest::Approx(-400.0f));
    CHECK(top_left[1] == doctest::Approx(300.0f));

    camera.set_zoom(2.0f);
    camera.set_position(Vec2f(5.0f, -3.0f));
    camera.set_rotation(0.4f);
    const Vec2f screen(123.0f, 456.0f);
    const Vec2f world = camera.screen_to_world(screen);
    const Vec2f back = camera.world_to_screen(world);
    CHECK(back[0] == doctest::Approx(screen[0]).epsilon(1e-3));
    CHECK(back[1] == doctest::Approx(screen[1]).epsilon(1e-3));
}

TEST_CASE("Camera2D rejects a non-positive viewport or zoom")
{
    CHECK_THROWS_AS(Camera2D(0.0f, 10.0f), Error);
    Camera2D camera(10.0f, 10.0f);
    CHECK_THROWS_AS(camera.set_zoom(0.0f), Error);
    CHECK_THROWS_AS(camera.set_viewport(10.0f, -1.0f), Error);
}
