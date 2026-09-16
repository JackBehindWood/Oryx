#pragma once

#include "Oryx/Math/Vector.h"

namespace oryx
{

using Vector3f = Vector<3, float>;
using Vector3d = Vector<3, double>;
using Vector3i = Vector<3, int>;

template<typename T>
constexpr Vector<3, T> cross(const Vector<3, T>& a, const Vector<3, T>& b)
{
    Vector<3, T> result;
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
    return result;
}

} // namespace oryx
