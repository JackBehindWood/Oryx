#include "oxpch.h"
#include "Oryx/Interface/Canvas/Animation.h"

namespace oryx
{

namespace
{

float bezier_at(float a, float b, float t)
{
    const float u = 1.0f - t;
    return 3.0f * u * u * t * a + 3.0f * u * t * t * b + t * t * t;
}

// Progress for the x of a CSS cubic-bezier, found by bisection (twelve steps is under a thousandth).
float bezier_ease(const float (&p)[4], float x)
{
    float low = 0.0f;
    float high = 1.0f;
    for (uint32_t step = 0; step < 12; ++step)
    {
        const float mid = (low + high) * 0.5f;
        (bezier_at(p[0], p[2], mid) < x ? low : high) = mid;
    }
    return bezier_at(p[1], p[3], (low + high) * 0.5f);
}

} // namespace

float ease(const Easing& easing, float t)
{
    t = math::clamp(t, 0.0f, 1.0f);
    if (t <= 0.0f || t >= 1.0f)
    {
        return t;
    }
    switch (easing.kind)
    {
        case Easing::Kind::Linear: return t;
        case Easing::Kind::EaseIn: return t * t * t;
        case Easing::Kind::EaseOut:
        {
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }
        case Easing::Kind::EaseInOut: return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
        case Easing::Kind::Bezier: return bezier_ease(easing.p, t);
        case Easing::Kind::Spring: return 1.0f - std::exp(-easing.p[0] * t) * std::cos(easing.p[1] * t) * (1.0f - t);
    }
    return t;
}

} // namespace oryx
