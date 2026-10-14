#pragma once

#include "Oryx/Math/Colour.h"

namespace oryx
{

// A curve as plain data: progress in [0, 1] in, eased progress out. Bezier takes the two control points of a CSS cubic-bezier; Spring a damping and a frequency and may overshoot 1 on the way.
struct Easing
{
    enum class Kind : uint8_t
    {
        Linear,
        EaseIn,
        EaseOut,
        EaseInOut,
        Bezier,
        Spring
    };

    Kind kind = Kind::EaseOut;
    float p[4] = {};

    [[nodiscard]] static constexpr Easing linear() { return { Kind::Linear, {} }; }
    [[nodiscard]] static constexpr Easing ease_in() { return { Kind::EaseIn, {} }; }
    [[nodiscard]] static constexpr Easing ease_out() { return { Kind::EaseOut, {} }; }
    [[nodiscard]] static constexpr Easing ease_in_out() { return { Kind::EaseInOut, {} }; }
    [[nodiscard]] static constexpr Easing bezier(float x1, float y1, float x2, float y2) { return { Kind::Bezier, { x1, y1, x2, y2 } }; }
    [[nodiscard]] static constexpr Easing spring(float damping = 6.0f, float frequency = 9.0f) { return { Kind::Spring, { damping, frequency, 0.0f, 0.0f } }; }
};

static_assert(std::is_trivially_copyable_v<Easing>);

// Eased progress for `t`, clamped to [0, 1] first; exactly 0 at 0 and 1 at 1 for every kind.
[[nodiscard]] float ease(const Easing& easing, float t);

// How a value travels to a new target.
struct AnimSpec
{
    float seconds = 0.12f;
    Easing easing = Easing::ease_out();

    // Hover and press fills.
    [[nodiscard]] static constexpr AnimSpec quick() { return { 0.10f, Easing::ease_out() }; }
    // Panels, toasts and anything that moves.
    [[nodiscard]] static constexpr AnimSpec standard() { return { 0.18f, Easing::ease_out() }; }
    [[nodiscard]] static constexpr AnimSpec slow() { return { 0.30f, Easing::ease_in_out() }; }
};

static_assert(std::is_trivially_copyable_v<AnimSpec>);

} // namespace oryx
