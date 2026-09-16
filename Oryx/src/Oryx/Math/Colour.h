#pragma once

#include "Oryx/Math/Functions.h"

namespace oryx
{

struct Colour
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

constexpr bool operator==(const Colour& a, const Colour& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

constexpr bool operator!=(const Colour& a, const Colour& b)
{
    return !(a == b);
}

inline bool approx_equal(const Colour& a, const Colour& b, float epsilon = math::EPSILON<float>)
{
    return math::approx_equal(a.r, b.r, epsilon)
        && math::approx_equal(a.g, b.g, epsilon)
        && math::approx_equal(a.b, b.b, epsilon)
        && math::approx_equal(a.a, b.a, epsilon);
}

constexpr Colour lerp(const Colour& a, const Colour& b, float t)
{
    return Colour{
        math::lerp(a.r, b.r, t),
        math::lerp(a.g, b.g, t),
        math::lerp(a.b, b.b, t),
        math::lerp(a.a, b.a, t)
    };
}

} // namespace oryx
