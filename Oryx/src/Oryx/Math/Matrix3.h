#pragma once

#include "Oryx/Math/Functions.h"
#include "Oryx/Math/Matrix.h"
#include "Oryx/Math/Vector2.h"
#include "Oryx/Math/Vector3.h"

namespace oryx
{

using Mat3f = Matrix<3, 3, float>;
using Mat3d = Matrix<3, 3, double>;

template<typename T>
constexpr Matrix<3, 3, T> translation(const Vector<2, T>& t)
{
    Matrix<3, 3, T> result = Matrix<3, 3, T>::identity();
    result.at(0, 2) = t[0];
    result.at(1, 2) = t[1];
    return result;
}

template<typename T>
Matrix<3, 3, T> rotation(T angle_radians)
{
    T c = math::cos(angle_radians);
    T s = math::sin(angle_radians);

    Matrix<3, 3, T> result = Matrix<3, 3, T>::identity();
    result.at(0, 0) = c; result.at(0, 1) = -s;
    result.at(1, 0) = s; result.at(1, 1) = c;
    return result;
}

template<typename T>
constexpr Matrix<3, 3, T> scale(const Vector<2, T>& s)
{
    Matrix<3, 3, T> result = Matrix<3, 3, T>::identity();
    result.at(0, 0) = s[0];
    result.at(1, 1) = s[1];
    return result;
}

// Affine-only (no perspective divide): treats p as a homogeneous (x, y, 1) point.
template<typename T>
constexpr Vector<2, T> transform_point(const Matrix<3, 3, T>& m, const Vector<2, T>& p)
{
    Vector<3, T> homogeneous(p[0], p[1], T{ 1 });
    Vector<3, T> transformed = m * homogeneous;
    return Vector<2, T>(transformed[0], transformed[1]);
}

} // namespace oryx
