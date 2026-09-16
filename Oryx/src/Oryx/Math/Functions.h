#pragma once

#include <cmath>

namespace oryx::math
{

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
T min(T a, T b) { return a < b ? a : b; }

template<typename T>
T max(T a, T b) { return a > b ? a : b; }

template<typename T>
T clamp(T value, T low, T high) { return min(max(value, low), high); }

template<typename T>
T lerp(T a, T b, T t) { return a + (b - a) * t; }

} // namespace oryx::math
