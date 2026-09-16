#pragma once

#include "Oryx/Math/Vector.h"

namespace oryx
{

using Vec2f = Vector<2, float>;
using Vec2d = Vector<2, double>;
using Vec2i = Vector<2, int>;

template<typename T>
constexpr T cross(const Vector<2, T>& a, const Vector<2, T>& b)
{
    return a[0] * b[1] - a[1] * b[0];
}

template<typename T>
constexpr T manhattan_distance(const Vector<2, T>& a, const Vector<2, T>& b)
{
    T dx = a[0] - b[0];
    T dy = a[1] - b[1];
    return (dx < T{ 0 } ? -dx : dx) + (dy < T{ 0 } ? -dy : dy);
}

template<typename T>
constexpr T chebyshev_distance(const Vector<2, T>& a, const Vector<2, T>& b)
{
    T dx = a[0] - b[0];
    T dy = a[1] - b[1];
    dx = dx < T{ 0 } ? -dx : dx;
    dy = dy < T{ 0 } ? -dy : dy;
    return dx > dy ? dx : dy;
}

} // namespace oryx
