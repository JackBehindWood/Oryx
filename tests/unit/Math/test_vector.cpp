#include "doctest.h"

#include <cmath>

#include "Oryx.h"

TEST_CASE("Vector arithmetic operators are componentwise")
{
    oryx::Vec2i a(1, 2);
    oryx::Vec2i b(3, 4);

    CHECK((a + b) == oryx::Vec2i(4, 6));
    CHECK((a - b) == oryx::Vec2i(-2, -2));
    CHECK((a * 2) == oryx::Vec2i(2, 4));
    CHECK((oryx::Vec2i(4, 6) / 2) == oryx::Vec2i(2, 3));
}

TEST_CASE("Vector dot, length and normalize match known values")
{
    oryx::Vec2f v(3.0f, 4.0f);

    CHECK(oryx::dot(v, v) == doctest::Approx(25.0f));
    CHECK(v.length() == doctest::Approx(5.0f));
    CHECK(oryx::length(v) == doctest::Approx(5.0f));

    oryx::Vec2f n = v.normalized();
    CHECK(n.length() == doctest::Approx(1.0f));
}

TEST_CASE("Vector2 and Vector3 cross products compute correctly")
{
    oryx::Vec2f a(1.0f, 0.0f);
    oryx::Vec2f b(0.0f, 1.0f);
    CHECK(oryx::cross(a, b) == doctest::Approx(1.0f));

    oryx::Vec3f x(1.0f, 0.0f, 0.0f);
    oryx::Vec3f y(0.0f, 1.0f, 0.0f);
    oryx::Vec3f z = oryx::cross(x, y);
    CHECK(z.x() == doctest::Approx(0.0f));
    CHECK(z.y() == doctest::Approx(0.0f));
    CHECK(z.z() == doctest::Approx(1.0f));
}

TEST_CASE("Vector alias types instantiate with expected underlying types")
{
    oryx::Vec2f vf;
    oryx::Vec2d vd;
    oryx::Vec2i vi;
    oryx::Vec3f v3f;

    CHECK(sizeof(vf[0]) == sizeof(float));
    CHECK(sizeof(vd[0]) == sizeof(double));
    CHECK(sizeof(vi[0]) == sizeof(int));
    CHECK(sizeof(v3f[0]) == sizeof(float));
}

TEST_CASE("Vec4 aliases instantiate and w() accesses the fourth component")
{
    oryx::Vec4f v(1.0f, 2.0f, 3.0f, 4.0f);
    CHECK(v.x() == doctest::Approx(1.0f));
    CHECK(v.w() == doctest::Approx(4.0f));

    oryx::Vec4i vi;
    CHECK(sizeof(vi[0]) == sizeof(int));
}

TEST_CASE("Vector compound assignment operators mutate in place")
{
    oryx::Vec2i v(1, 2);
    v += oryx::Vec2i(3, 4);
    CHECK(v == oryx::Vec2i(4, 6));

    v -= oryx::Vec2i(1, 1);
    CHECK(v == oryx::Vec2i(3, 5));

    v *= 2;
    CHECK(v == oryx::Vec2i(6, 10));

    v *= oryx::Vec2i(2, 3);
    CHECK(v == oryx::Vec2i(12, 30));

    v /= 2;
    CHECK(v == oryx::Vec2i(6, 15));

    v /= oryx::Vec2i(3, 5);
    CHECK(v == oryx::Vec2i(2, 3));
}

TEST_CASE("Unary minus negates every component")
{
    oryx::Vec2i v(3, -4);
    CHECK(-v == oryx::Vec2i(-3, 4));
}

TEST_CASE("operator!= is the negation of operator==")
{
    oryx::Vec2i a(1, 2);
    oryx::Vec2i b(1, 2);
    oryx::Vec2i c(1, 3);

    CHECK_FALSE(a != b);
    CHECK(a != c);
}

TEST_CASE("Componentwise vector*vector and vector/vector operate elementwise")
{
    oryx::Vec2i a(2, 6);
    oryx::Vec2i b(3, 2);

    CHECK((a * b) == oryx::Vec2i(6, 12));
    CHECK((a / b) == oryx::Vec2i(0, 3));

    CHECK((oryx::Vec2i(1, 1) / oryx::Vec2i(2, 2)) == oryx::Vec2i(0, 0));
}

TEST_CASE("distance and distance_squared match known values")
{
    oryx::Vec2f a(0.0f, 0.0f);
    oryx::Vec2f b(3.0f, 4.0f);

    CHECK(oryx::distance_squared(a, b) == doctest::Approx(25.0f));
    CHECK(oryx::distance(a, b) == doctest::Approx(5.0f));
    CHECK(a.distance(b) == doctest::Approx(5.0f));
    CHECK(a.distance_squared(b) == doctest::Approx(25.0f));
}

TEST_CASE("Vector lerp, clamp, min and max operate componentwise")
{
    oryx::Vec2f a(0.0f, 0.0f);
    oryx::Vec2f b(10.0f, 20.0f);

    CHECK(oryx::lerp(a, b, 0.5f) == oryx::Vec2f(5.0f, 10.0f));

    oryx::Vec2f v(-1.0f, 15.0f);
    CHECK(oryx::clamp(v, a, b) == oryx::Vec2f(0.0f, 15.0f));

    CHECK(oryx::min(a, b) == a);
    CHECK(oryx::max(a, b) == b);
}

TEST_CASE("Vector abs takes the absolute value of every component")
{
    oryx::Vec2i v(-3, 4);
    CHECK(oryx::abs(v) == oryx::Vec2i(3, 4));
}

TEST_CASE("approx_equal treats near-equal vectors as equal within epsilon")
{
    oryx::Vec2f a(1.0f, 1.0f);
    oryx::Vec2f b(1.0f + 1e-6f, 1.0f);
    oryx::Vec2f c(1.1f, 1.0f);

    CHECK(oryx::approx_equal(a, b));
    CHECK_FALSE(oryx::approx_equal(a, c));
}

TEST_CASE("Normalizing the zero vector produces the documented NaN/Inf result")
{
    oryx::Vec2f zero(0.0f, 0.0f);
    oryx::Vec2f n = zero.normalized();

    CHECK(std::isnan(n.x()));
    CHECK(std::isnan(n.y()));
}

TEST_CASE("to_string formats a Vector's components")
{
    oryx::Vec2i v(1, 2);
    CHECK(oryx::to_string(v) == "(1, 2)");
}
