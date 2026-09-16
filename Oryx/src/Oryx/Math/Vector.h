#pragma once

#include <concepts>
#include <string>
#include <type_traits>

#include "Oryx/Math/Functions.h"

namespace oryx
{

template<size_t N, typename T>
class Vector
{
public:
    Vector() = default;

    explicit constexpr Vector(T value)
    {
        for (size_t i = 0; i < N; ++i)
        {
            m_data[i] = value;
        }
    }

    template<typename... Args>
    requires (sizeof...(Args) == N) && (std::convertible_to<Args, T> && ...)
    constexpr Vector(Args... args) : m_data{ static_cast<T>(args)... } {}

    T& operator[](size_t i) { return m_data[i]; }
    const T& operator[](size_t i) const { return m_data[i]; }

    T& x() requires (N >= 1) { return m_data[0]; }
    T& y() requires (N >= 2) { return m_data[1]; }
    T& z() requires (N >= 3) { return m_data[2]; }
    T& w() requires (N >= 4) { return m_data[3]; }
    const T& x() const requires (N >= 1) { return m_data[0]; }
    const T& y() const requires (N >= 2) { return m_data[1]; }
    const T& z() const requires (N >= 3) { return m_data[2]; }
    const T& w() const requires (N >= 4) { return m_data[3]; }

    constexpr Vector& operator+=(const Vector& other)
    {
        for (size_t i = 0; i < N; ++i) { m_data[i] += other[i]; }
        return *this;
    }

    constexpr Vector& operator-=(const Vector& other)
    {
        for (size_t i = 0; i < N; ++i) { m_data[i] -= other[i]; }
        return *this;
    }

    constexpr Vector& operator*=(T scalar)
    {
        for (size_t i = 0; i < N; ++i) { m_data[i] *= scalar; }
        return *this;
    }

    constexpr Vector& operator*=(const Vector& other)
    {
        for (size_t i = 0; i < N; ++i) { m_data[i] *= other[i]; }
        return *this;
    }

    constexpr Vector& operator/=(T scalar)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            // Reciprocal-multiply is only valid for floating-point T — for
            // integers, 1/scalar truncates to 0 for any |scalar| > 1.
            const T inv_scalar = T{ 1 } / scalar;
            for (size_t i = 0; i < N; ++i)
            {
                m_data[i] = m_data[i] * inv_scalar;
            }
        }
        else
        {
            for (size_t i = 0; i < N; ++i)
            {
                m_data[i] = m_data[i] / scalar;
            }
        }
        return *this;
    }

    constexpr Vector& operator/=(const Vector& other)
    {
        for (size_t i = 0; i < N; ++i) { m_data[i] /= other[i]; }
        return *this;
    }

    T sum() const;
    T mean() const;
    T length() const;
    T length_squared() const;
    Vector<N, T> normalized() const;

    // distance()/distance_squared() get member forms (unlike dot()/cross(), which stay
    // free-function-only) as a deliberate exception, matching length()/normalized() etc.
    T distance(const Vector& other) const;
    constexpr T distance_squared(const Vector& other) const;

private:
    T m_data[N]{};
};

template<size_t N, typename T>
constexpr Vector<N, T> operator+(const Vector<N, T>& a, const Vector<N, T>& b)
{
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = a[i] + b[i];
    }
    return result;
}

template<size_t N, typename T>
constexpr Vector<N, T> operator-(const Vector<N, T>& a, const Vector<N, T>& b)
{
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = a[i] - b[i];
    }
    return result;
}

template<size_t N, typename T>
constexpr Vector<N, T> operator-(const Vector<N, T>& v)
{
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = -v[i];
    }
    return result;
}

template<size_t N, typename T>
constexpr Vector<N, T> operator*(const Vector<N, T>& v, T scalar)
{
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = v[i] * scalar;
    }
    return result;
}

template<size_t N, typename T>
constexpr Vector<N, T> operator*(const Vector<N, T>& a, const Vector<N, T>& b)
{
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = a[i] * b[i];
    }
    return result;
}

template<size_t N, typename T>
constexpr Vector<N, T> operator/(const Vector<N, T>& v, T scalar)
{
    Vector<N, T> result;
    if constexpr (std::is_floating_point_v<T>)
    {
        // Reciprocal-multiply is only valid for floating-point T — for
        // integers, 1/scalar truncates to 0 for any |scalar| > 1.
        const T inv_scalar = T{ 1 } / scalar;
        for (size_t i = 0; i < N; ++i)
        {
            result[i] = v[i] * inv_scalar;
        }
    }
    else
    {
        for (size_t i = 0; i < N; ++i)
        {
            result[i] = v[i] / scalar;
        }
    }
    return result;
}

template<size_t N, typename T>
constexpr Vector<N, T> operator/(const Vector<N, T>& a, const Vector<N, T>& b)
{
    // Each component has a different divisor, so the scalar reciprocal-multiply
    // trick used by operator/(Vector, T) doesn't apply here.
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = a[i] / b[i];
    }
    return result;
}

template<size_t N, typename T>
constexpr bool operator==(const Vector<N, T>& a, const Vector<N, T>& b)
{
    for (size_t i = 0; i < N; ++i)
    {
        if (!(a[i] == b[i]))
        {
            return false;
        }
    }
    return true;
}

template<size_t N, typename T>
constexpr bool operator!=(const Vector<N, T>& a, const Vector<N, T>& b)
{
    return !(a == b);
}

template<size_t N, typename T>
constexpr T sum(const Vector<N, T>& v)
{
    T result{};
    for (size_t i = 0; i < N; ++i)
    {
        result += v[i];
    }
    return result;
}

template<size_t N, typename T>
T mean(const Vector<N, T>& v)
{
    return sum(v) / static_cast<T>(N);
}

template<size_t N, typename T>
constexpr T dot(const Vector<N, T>& a, const Vector<N, T>& b)
{
    T result{};
    for (size_t i = 0; i < N; ++i)
    {
        result += a[i] * b[i];
    }
    return result;
}

template<size_t N, typename T>
T length(const Vector<N, T>& v)
{
    return math::sqrt(dot(v, v));
}

template<size_t N, typename T>
T length_squared(const Vector<N, T>& v)
{
    return dot(v, v);
}

template<size_t N, typename T>
Vector<N, T> normalize(const Vector<N, T>& v)
{
    return v / length(v);
}

template<size_t N, typename T>
constexpr T distance_squared(const Vector<N, T>& a, const Vector<N, T>& b)
{
    return dot(a - b, a - b);
}

template<size_t N, typename T>
T distance(const Vector<N, T>& a, const Vector<N, T>& b)
{
    return math::sqrt(distance_squared(a, b));
}

template<size_t N, typename T>
constexpr Vector<N, T> lerp(const Vector<N, T>& a, const Vector<N, T>& b, T t)
{
    return a + (b - a) * t;
}

template<size_t N, typename T>
constexpr Vector<N, T> clamp(const Vector<N, T>& v, const Vector<N, T>& low, const Vector<N, T>& high)
{
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = math::clamp(v[i], low[i], high[i]);
    }
    return result;
}

template<size_t N, typename T>
constexpr Vector<N, T> min(const Vector<N, T>& a, const Vector<N, T>& b)
{
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = math::min(a[i], b[i]);
    }
    return result;
}

template<size_t N, typename T>
constexpr Vector<N, T> max(const Vector<N, T>& a, const Vector<N, T>& b)
{
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = math::max(a[i], b[i]);
    }
    return result;
}

template<size_t N, typename T>
Vector<N, T> abs(const Vector<N, T>& v)
{
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = math::abs(v[i]);
    }
    return result;
}

template<size_t N, typename T>
bool approx_equal(const Vector<N, T>& a, const Vector<N, T>& b, T epsilon = math::EPSILON<T>)
{
    for (size_t i = 0; i < N; ++i)
    {
        if (!math::approx_equal(a[i], b[i], epsilon))
        {
            return false;
        }
    }
    return true;
}

template<size_t N, typename T>
std::string to_string(const Vector<N, T>& v)
{
    std::string result = "(";
    for (size_t i = 0; i < N; ++i)
    {
        result += std::to_string(v[i]);
        if (i + 1 < N)
        {
            result += ", ";
        }
    }
    result += ")";
    return result;
}

template<size_t N, typename T>
T Vector<N, T>::sum() const { return oryx::sum(*this); }

template<size_t N, typename T>
T Vector<N, T>::mean() const { return oryx::mean(*this); }

template<size_t N, typename T>
T Vector<N, T>::length() const { return oryx::length(*this); }

template<size_t N, typename T>
T Vector<N, T>::length_squared() const { return oryx::length_squared(*this); }

template<size_t N, typename T>
Vector<N, T> Vector<N, T>::normalized() const { return oryx::normalize(*this); }

template<size_t N, typename T>
T Vector<N, T>::distance(const Vector& other) const { return oryx::distance(*this, other); }

template<size_t N, typename T>
constexpr T Vector<N, T>::distance_squared(const Vector& other) const { return oryx::distance_squared(*this, other); }

} // namespace oryx
