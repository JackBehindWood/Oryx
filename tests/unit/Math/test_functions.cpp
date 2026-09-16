#include "doctest.h"

#include "Oryx.h"

TEST_CASE("math constants have the expected relationships")
{
    CHECK(oryx::math::TWO_PI<float> == doctest::Approx(2.0f * oryx::math::PI<float>));
    CHECK(oryx::math::HALF_PI<float> == doctest::Approx(oryx::math::PI<float> / 2.0f));
    CHECK(oryx::math::EPSILON<float> > 0.0f);
    CHECK(oryx::math::EPSILON<float> < 0.01f);
}

TEST_CASE("math::sign returns -1, 0, or 1")
{
    CHECK(oryx::math::sign(5.0f) == doctest::Approx(1.0f));
    CHECK(oryx::math::sign(-5.0f) == doctest::Approx(-1.0f));
    CHECK(oryx::math::sign(0.0f) == doctest::Approx(0.0f));
}

TEST_CASE("math::saturate clamps to [0,1]")
{
    CHECK(oryx::math::saturate(-0.5f) == doctest::Approx(0.0f));
    CHECK(oryx::math::saturate(0.5f) == doctest::Approx(0.5f));
    CHECK(oryx::math::saturate(1.5f) == doctest::Approx(1.0f));
}

TEST_CASE("math::smoothstep interpolates smoothly between edges")
{
    CHECK(oryx::math::smoothstep(0.0f, 1.0f, 0.0f) == doctest::Approx(0.0f));
    CHECK(oryx::math::smoothstep(0.0f, 1.0f, 1.0f) == doctest::Approx(1.0f));
    CHECK(oryx::math::smoothstep(0.0f, 1.0f, 0.5f) == doctest::Approx(0.5f));
}

TEST_CASE("math::radians and math::degrees convert between known values")
{
    CHECK(oryx::math::radians(180.0f) == doctest::Approx(oryx::math::PI<float>));
    CHECK(oryx::math::degrees(oryx::math::PI<float>) == doctest::Approx(180.0f));
}

TEST_CASE("math::approx_equal respects the epsilon threshold")
{
    CHECK(oryx::math::approx_equal(1.0f, 1.0f + 1e-6f));
    CHECK_FALSE(oryx::math::approx_equal(1.0f, 1.1f));
    CHECK(oryx::math::approx_equal(1.0f, 1.05f, 0.1f));
}

TEST_CASE("math::min, max, clamp and lerp behave as expected")
{
    CHECK(oryx::math::min(3, 5) == 3);
    CHECK(oryx::math::max(3, 5) == 5);
    CHECK(oryx::math::clamp(10, 0, 5) == 5);
    CHECK(oryx::math::clamp(-10, 0, 5) == 0);
    CHECK(oryx::math::lerp(0.0f, 10.0f, 0.5f) == doctest::Approx(5.0f));
}

TEST_CASE("math wrappers over <cmath> match known values")
{
    CHECK(oryx::math::sqrt(9.0f) == doctest::Approx(3.0f));
    CHECK(oryx::math::abs(-3.0f) == doctest::Approx(3.0f));
    CHECK(oryx::math::pow(2.0f, 3.0f) == doctest::Approx(8.0f));
}
