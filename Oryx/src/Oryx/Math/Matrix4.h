#pragma once

#include "Oryx/Math/Functions.h"
#include "Oryx/Math/Matrix.h"
#include "Oryx/Math/Vector3.h"
#include "Oryx/Math/Vector4.h"

namespace oryx
{

// Row-major, column-vector (m * v), right-handed view space looking down -Z, clip depth in [0, 1].
enum class DepthConvention : uint8_t
{
    Standard,
    ReverseZ,
};

template<typename T>
constexpr Matrix<4, 4, T> translation(const Vector<3, T>& t)
{
    Matrix<4, 4, T> result = Matrix<4, 4, T>::identity();
    result.at(0, 3) = t[0];
    result.at(1, 3) = t[1];
    result.at(2, 3) = t[2];
    return result;
}

template<typename T>
constexpr Matrix<4, 4, T> scale(const Vector<3, T>& s)
{
    Matrix<4, 4, T> result = Matrix<4, 4, T>::identity();
    result.at(0, 0) = s[0];
    result.at(1, 1) = s[1];
    result.at(2, 2) = s[2];
    return result;
}

// Right-handed rotation of `angle_radians` about `axis`, which need not be normalised.
template<typename T>
Matrix<4, 4, T> rotation(const Vector<3, T>& axis, T angle_radians)
{
    Vector<3, T> n = normalize(axis);
    T c = math::cos(angle_radians);
    T s = math::sin(angle_radians);
    T t = T{ 1 } - c;

    Matrix<4, 4, T> result = Matrix<4, 4, T>::identity();
    result.at(0, 0) = c + n[0] * n[0] * t;
    result.at(0, 1) = n[0] * n[1] * t - n[2] * s;
    result.at(0, 2) = n[0] * n[2] * t + n[1] * s;
    result.at(1, 0) = n[1] * n[0] * t + n[2] * s;
    result.at(1, 1) = c + n[1] * n[1] * t;
    result.at(1, 2) = n[1] * n[2] * t - n[0] * s;
    result.at(2, 0) = n[2] * n[0] * t - n[1] * s;
    result.at(2, 1) = n[2] * n[1] * t + n[0] * s;
    result.at(2, 2) = c + n[2] * n[2] * t;
    return result;
}

// near_plane/far_plane are positive distances in front of the eye; ReverseZ maps near to 1 and far to 0.
template<typename T>
Matrix<4, 4, T> orthographic(T left, T right, T bottom, T top, T near_plane, T far_plane, DepthConvention depth = DepthConvention::Standard)
{
    if (depth == DepthConvention::ReverseZ)
    {
        std::swap(near_plane, far_plane);
    }
    Matrix<4, 4, T> result = Matrix<4, 4, T>::identity();
    result.at(0, 0) = T{ 2 } / (right - left);
    result.at(1, 1) = T{ 2 } / (top - bottom);
    result.at(2, 2) = T{ 1 } / (near_plane - far_plane);
    result.at(0, 3) = -(right + left) / (right - left);
    result.at(1, 3) = -(top + bottom) / (top - bottom);
    result.at(2, 3) = near_plane / (near_plane - far_plane);
    return result;
}

template<typename T>
Matrix<4, 4, T> perspective(T fov_y_radians, T aspect, T near_plane, T far_plane, DepthConvention depth = DepthConvention::Standard)
{
    if (depth == DepthConvention::ReverseZ)
    {
        std::swap(near_plane, far_plane);
    }
    T f = T{ 1 } / math::tan(fov_y_radians / T{ 2 });
    Matrix<4, 4, T> result;
    result.at(0, 0) = f / aspect;
    result.at(1, 1) = f;
    result.at(2, 2) = far_plane / (near_plane - far_plane);
    result.at(2, 3) = near_plane * far_plane / (near_plane - far_plane);
    result.at(3, 2) = T{ -1 };
    return result;
}

template<typename T>
Matrix<4, 4, T> look_at(const Vector<3, T>& eye, const Vector<3, T>& target, const Vector<3, T>& up)
{
    Vector<3, T> forward = normalize(target - eye);
    Vector<3, T> side = normalize(cross(forward, up));
    Vector<3, T> true_up = cross(side, forward);

    Matrix<4, 4, T> result = Matrix<4, 4, T>::identity();
    for (size_t i = 0; i < 3; ++i)
    {
        result.at(0, i) = side[i];
        result.at(1, i) = true_up[i];
        result.at(2, i) = -forward[i];
    }
    result.at(0, 3) = -dot(side, eye);
    result.at(1, 3) = -dot(true_up, eye);
    result.at(2, 3) = dot(forward, eye);
    return result;
}

// Packs for a GPU that reads matrices column by column (MSL float4x4).
inline void to_column_major(const Mat4f& m, float (&out)[16])
{
    for (size_t col = 0; col < 4; ++col)
    {
        for (size_t row = 0; row < 4; ++row)
        {
            out[col * 4 + row] = m.at(row, col);
        }
    }
}

} // namespace oryx
