#pragma once

#include "Oryx/Math/Vector.h"

namespace oryx
{

using Vector2f = Vector<2, float>;
using Vector2d = Vector<2, double>;
using Vector2i = Vector<2, int>;

template<typename T>
constexpr T cross(const Vector<2, T>& a, const Vector<2, T>& b)
{
    return a[0] * b[1] - a[1] * b[0];
}

} // namespace oryx
