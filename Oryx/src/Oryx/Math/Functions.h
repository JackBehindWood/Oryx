#pragma once

#include <cmath>

namespace oryx::math
{

template<typename T>
inline constexpr T PI = static_cast<T>(3.14159265358979323846);

template<typename T>
inline constexpr T TWO_PI = PI<T> * static_cast<T>(2);

template<typename T>
inline constexpr T HALF_PI = PI<T> / static_cast<T>(2);

template<typename T>
inline constexpr T EPSILON = static_cast<T>(1e-5);

template<typename T>
T sqrt(T value) { return static_cast<T>(std::sqrt(value)); }

template<typename T>
T abs(T value) { return static_cast<T>(std::abs(value)); }

template<typename T>
T sin(T value) { return static_cast<T>(std::sin(value)); }

template<typename T>
T cos(T value) { return static_cast<T>(std::cos(value)); }

template<typename T>
T pow(T base, T exponent) { return static_cast<T>(std::pow(base, exponent)); }

template<typename T>
T tan(T value) { return static_cast<T>(std::tan(value)); }

template<typename T>
T asin(T value) { return static_cast<T>(std::asin(value)); }

template<typename T>
T acos(T value) { return static_cast<T>(std::acos(value)); }

template<typename T>
T atan(T value) { return static_cast<T>(std::atan(value)); }

template<typename T>
T atan2(T y, T x) { return static_cast<T>(std::atan2(y, x)); }

template<typename T>
T floor(T value) { return static_cast<T>(std::floor(value)); }

template<typename T>
T ceil(T value) { return static_cast<T>(std::ceil(value)); }

template<typename T>
T round(T value) { return static_cast<T>(std::round(value)); }

template<typename T>
T exp(T value) { return static_cast<T>(std::exp(value)); }

template<typename T>
T log(T value) { return static_cast<T>(std::log(value)); }

template<typename T>
T log2(T value) { return static_cast<T>(std::log2(value)); }

template<typename T>
constexpr T min(T a, T b) { return a < b ? a : b; }

template<typename T>
constexpr T max(T a, T b) { return a > b ? a : b; }

template<typename T>
constexpr T clamp(T value, T low, T high) { return min(max(value, low), high); }

template<typename T>
constexpr T lerp(T a, T b, T t) { return a + (b - a) * t; }

template<typename T>
constexpr T sign(T value) { return value > T{ 0 } ? T{ 1 } : (value < T{ 0 } ? T{ -1 } : T{ 0 }); }

template<typename T>
constexpr T saturate(T value) { return clamp(value, T{ 0 }, T{ 1 }); }

template<typename T>
constexpr T smoothstep(T edge0, T edge1, T x)
{
    T t = saturate((x - edge0) / (edge1 - edge0));
    return t * t * (T{ 3 } - T{ 2 } * t);
}

template<typename T>
constexpr T radians(T degrees_value) { return degrees_value * PI<T> / T{ 180 }; }

template<typename T>
constexpr T degrees(T radians_value) { return radians_value * T{ 180 } / PI<T>; }

template<typename T>
bool approx_equal(T a, T b, T epsilon = EPSILON<T>) { return abs(a - b) <= epsilon; }

} // namespace oryx::math
