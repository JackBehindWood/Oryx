#pragma once

#include <string>

#include "Oryx/Core/Assert.h"
#include "Oryx/Math/Functions.h"
#include "Oryx/Math/Vector.h"

namespace oryx
{

template<size_t R, size_t C, typename T>
class Matrix
{
public:
    Matrix() = default;

    static constexpr Matrix<R, C, T> identity()
    {
        static_assert(R == C, "Matrix::identity() requires a square matrix");
        Matrix<R, C, T> result;
        for (size_t i = 0; i < R; ++i)
        {
            result.at(i, i) = T{ 1 };
        }
        return result;
    }

    T& at(size_t row, size_t col) { return m_data[row * C + col]; }
    const T& at(size_t row, size_t col) const { return m_data[row * C + col]; }

    constexpr Matrix<C, R, T> transpose() const;
    T determinant() const requires ((R == 2 && C == 2) || (R == 3 && C == 3));
    Matrix<R, C, T> inverse() const requires ((R == 2 && C == 2) || (R == 3 && C == 3));

private:
    T m_data[R * C]{};
};

using Mat2f = Matrix<2, 2, float>;
using Mat2d = Matrix<2, 2, double>;
using Mat4f = Matrix<4, 4, float>;
using Mat4d = Matrix<4, 4, double>;

template<size_t R, size_t K, size_t C, typename T>
constexpr Matrix<R, C, T> operator*(const Matrix<R, K, T>& a, const Matrix<K, C, T>& b)
{
    Matrix<R, C, T> result;
    for (size_t row = 0; row < R; ++row)
    {
        for (size_t col = 0; col < C; ++col)
        {
            T sum{};
            for (size_t k = 0; k < K; ++k)
            {
                sum += a.at(row, k) * b.at(k, col);
            }
            result.at(row, col) = sum;
        }
    }
    return result;
}

template<size_t R, size_t C, typename T>
constexpr Vector<R, T> operator*(const Matrix<R, C, T>& m, const Vector<C, T>& v)
{
    Vector<R, T> result;
    for (size_t row = 0; row < R; ++row)
    {
        T sum{};
        for (size_t col = 0; col < C; ++col)
        {
            sum += m.at(row, col) * v[col];
        }
        result[row] = sum;
    }
    return result;
}

template<size_t R, size_t C, typename T>
constexpr Matrix<R, C, T> operator+(const Matrix<R, C, T>& a, const Matrix<R, C, T>& b)
{
    Matrix<R, C, T> result;
    for (size_t row = 0; row < R; ++row)
    {
        for (size_t col = 0; col < C; ++col)
        {
            result.at(row, col) = a.at(row, col) + b.at(row, col);
        }
    }
    return result;
}

template<size_t R, size_t C, typename T>
constexpr Matrix<R, C, T> operator-(const Matrix<R, C, T>& a, const Matrix<R, C, T>& b)
{
    Matrix<R, C, T> result;
    for (size_t row = 0; row < R; ++row)
    {
        for (size_t col = 0; col < C; ++col)
        {
            result.at(row, col) = a.at(row, col) - b.at(row, col);
        }
    }
    return result;
}

template<size_t R, size_t C, typename T>
constexpr Matrix<R, C, T> operator*(const Matrix<R, C, T>& m, T scalar)
{
    Matrix<R, C, T> result;
    for (size_t row = 0; row < R; ++row)
    {
        for (size_t col = 0; col < C; ++col)
        {
            result.at(row, col) = m.at(row, col) * scalar;
        }
    }
    return result;
}

template<size_t R, size_t C, typename T>
constexpr bool operator==(const Matrix<R, C, T>& a, const Matrix<R, C, T>& b)
{
    for (size_t row = 0; row < R; ++row)
    {
        for (size_t col = 0; col < C; ++col)
        {
            if (!(a.at(row, col) == b.at(row, col)))
            {
                return false;
            }
        }
    }
    return true;
}

template<size_t R, size_t C, typename T>
constexpr Matrix<C, R, T> transpose(const Matrix<R, C, T>& m)
{
    Matrix<C, R, T> result;
    for (size_t row = 0; row < R; ++row)
    {
        for (size_t col = 0; col < C; ++col)
        {
            result.at(col, row) = m.at(row, col);
        }
    }
    return result;
}

// determinant()/inverse() are deliberately bounded to 2x2 and 3x3 via fully-specialized
// overloads — this makes calling them on any other size a compile error, rather than a
// general N x N implementation (out of scope; see docs/architecture.md §3.4).
template<typename T>
constexpr T determinant(const Matrix<2, 2, T>& m)
{
    return m.at(0, 0) * m.at(1, 1) - m.at(0, 1) * m.at(1, 0);
}

template<typename T>
constexpr T determinant(const Matrix<3, 3, T>& m)
{
    T a = m.at(0, 0), b = m.at(0, 1), c = m.at(0, 2);
    T d = m.at(1, 0), e = m.at(1, 1), f = m.at(1, 2);
    T g = m.at(2, 0), h = m.at(2, 1), i = m.at(2, 2);
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

// Asserts (debug builds only) that the matrix is non-singular, then proceeds regardless —
// in Release, where the assert compiles out, a singular matrix yields IEEE Inf/NaN, which is
// well-defined floating-point behavior. Callers who need to guard against this should check
// determinant() themselves first.
template<typename T>
Matrix<2, 2, T> inverse(const Matrix<2, 2, T>& m)
{
    T det = determinant(m);
    OX_CORE_ASSERT(math::abs(det) > math::EPSILON<T>, "Matrix::inverse() called on a singular (or near-singular) matrix");
    T inv_det = T{ 1 } / det;

    Matrix<2, 2, T> result;
    result.at(0, 0) = m.at(1, 1) * inv_det;
    result.at(0, 1) = -m.at(0, 1) * inv_det;
    result.at(1, 0) = -m.at(1, 0) * inv_det;
    result.at(1, 1) = m.at(0, 0) * inv_det;
    return result;
}

template<typename T>
Matrix<3, 3, T> inverse(const Matrix<3, 3, T>& m)
{
    T det = determinant(m);
    OX_CORE_ASSERT(math::abs(det) > math::EPSILON<T>, "Matrix::inverse() called on a singular (or near-singular) matrix");
    T inv_det = T{ 1 } / det;

    T a = m.at(0, 0), b = m.at(0, 1), c = m.at(0, 2);
    T d = m.at(1, 0), e = m.at(1, 1), f = m.at(1, 2);
    T g = m.at(2, 0), h = m.at(2, 1), i = m.at(2, 2);

    Matrix<3, 3, T> result;
    result.at(0, 0) = (e * i - f * h) * inv_det;
    result.at(0, 1) = (c * h - b * i) * inv_det;
    result.at(0, 2) = (b * f - c * e) * inv_det;
    result.at(1, 0) = (f * g - d * i) * inv_det;
    result.at(1, 1) = (a * i - c * g) * inv_det;
    result.at(1, 2) = (c * d - a * f) * inv_det;
    result.at(2, 0) = (d * h - e * g) * inv_det;
    result.at(2, 1) = (b * g - a * h) * inv_det;
    result.at(2, 2) = (a * e - b * d) * inv_det;
    return result;
}

template<size_t R, size_t C, typename T>
std::string to_string(const Matrix<R, C, T>& m)
{
    std::string result;
    for (size_t row = 0; row < R; ++row)
    {
        result += "[";
        for (size_t col = 0; col < C; ++col)
        {
            result += std::to_string(m.at(row, col));
            if (col + 1 < C)
            {
                result += ", ";
            }
        }
        result += "]";
        if (row + 1 < R)
        {
            result += "\n";
        }
    }
    return result;
}

template<size_t R, size_t C, typename T>
constexpr Matrix<C, R, T> Matrix<R, C, T>::transpose() const { return oryx::transpose(*this); }

template<size_t R, size_t C, typename T>
T Matrix<R, C, T>::determinant() const requires ((R == 2 && C == 2) || (R == 3 && C == 3)) { return oryx::determinant(*this); }

template<size_t R, size_t C, typename T>
Matrix<R, C, T> Matrix<R, C, T>::inverse() const requires ((R == 2 && C == 2) || (R == 3 && C == 3)) { return oryx::inverse(*this); }

} // namespace oryx
