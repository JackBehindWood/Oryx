#include "doctest.h"

#include "Oryx.h"

TEST_CASE("Colour default-constructs to opaque black")
{
    oryx::Colour c;
    CHECK(c.r == doctest::Approx(0.0f));
    CHECK(c.g == doctest::Approx(0.0f));
    CHECK(c.b == doctest::Approx(0.0f));
    CHECK(c.a == doctest::Approx(1.0f));
}

TEST_CASE("Colour equality compares all four channels")
{
    oryx::Colour a{ 0.1f, 0.2f, 0.3f, 0.4f };
    oryx::Colour b{ 0.1f, 0.2f, 0.3f, 0.4f };
    oryx::Colour c{ 0.1f, 0.2f, 0.3f, 0.5f };

    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("Colour approx_equal respects epsilon")
{
    oryx::Colour a{ 0.5f, 0.5f, 0.5f, 1.0f };
    oryx::Colour b{ 0.5f + 1e-6f, 0.5f, 0.5f, 1.0f };
    oryx::Colour c{ 0.6f, 0.5f, 0.5f, 1.0f };

    CHECK(oryx::approx_equal(a, b));
    CHECK_FALSE(oryx::approx_equal(a, c));
}

TEST_CASE("Colour lerp interpolates each channel")
{
    oryx::Colour a{ 0.0f, 0.0f, 0.0f, 0.0f };
    oryx::Colour b{ 1.0f, 1.0f, 1.0f, 1.0f };

    auto mid = oryx::lerp(a, b, 0.5f);
    CHECK(mid.r == doctest::Approx(0.5f));
    CHECK(mid.g == doctest::Approx(0.5f));
    CHECK(mid.b == doctest::Approx(0.5f));
    CHECK(mid.a == doctest::Approx(0.5f));
}
