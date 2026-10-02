#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

template<typename E>
inline constexpr bool rhi_flags_enum = false;

template<typename E>
    requires rhi_flags_enum<E>
constexpr E operator|(E a, E b)
{
    return static_cast<E>(static_cast<std::underlying_type_t<E>>(a) | static_cast<std::underlying_type_t<E>>(b));
}

template<typename E>
    requires rhi_flags_enum<E>
constexpr E operator&(E a, E b)
{
    return static_cast<E>(static_cast<std::underlying_type_t<E>>(a) & static_cast<std::underlying_type_t<E>>(b));
}

template<typename E>
    requires rhi_flags_enum<E>
constexpr E& operator|=(E& a, E b)
{
    return a = a | b;
}

template<typename E>
    requires rhi_flags_enum<E>
constexpr bool has_flag(E value, E flag)
{
    return (value & flag) == flag;
}

} // namespace oryx
