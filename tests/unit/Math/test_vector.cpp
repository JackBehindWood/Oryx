#include "doctest.h"

#include "Oryx.h"

TEST_CASE("Vector arithmetic operators are componentwise")
{
    oryx::Vector2i a(1, 2);
    oryx::Vector2i b(3, 4);

    CHECK((a + b) == oryx::Vector2i(4, 6));
    CHECK((a - b) == oryx::Vector2i(-2, -2));
    CHECK((a * 2) == oryx::Vector2i(2, 4));
    CHECK((oryx::Vector2i(4, 6) / 2) == oryx::Vector2i(2, 3));
}

TEST_CASE("Vector dot, length and normalize match known values")
{
    oryx::Vector2f v(3.0f, 4.0f);

    CHECK(oryx::dot(v, v) == doctest::Approx(25.0f));
    CHECK(v.length() == doctest::Approx(5.0f));
    CHECK(oryx::length(v) == doctest::Approx(5.0f));

    oryx::Vector2f n = v.normalized();
    CHECK(n.length() == doctest::Approx(1.0f));
}

TEST_CASE("Vector2 and Vector3 cross products compute correctly")
{
    oryx::Vector2f a(1.0f, 0.0f);
    oryx::Vector2f b(0.0f, 1.0f);
    CHECK(oryx::cross(a, b) == doctest::Approx(1.0f));

    oryx::Vector3f x(1.0f, 0.0f, 0.0f);
    oryx::Vector3f y(0.0f, 1.0f, 0.0f);
    oryx::Vector3f z = oryx::cross(x, y);
    CHECK(z.x() == doctest::Approx(0.0f));
    CHECK(z.y() == doctest::Approx(0.0f));
    CHECK(z.z() == doctest::Approx(1.0f));
}

TEST_CASE("Vector alias types instantiate with expected underlying types")
{
    oryx::Vector2f vf;
    oryx::Vector2d vd;
    oryx::Vector2i vi;
    oryx::Vector3f v3f;

    CHECK(sizeof(vf[0]) == sizeof(float));
    CHECK(sizeof(vd[0]) == sizeof(double));
    CHECK(sizeof(vi[0]) == sizeof(int));
    CHECK(sizeof(v3f[0]) == sizeof(float));
}
