#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

// A string literal usable as a non-type template parameter.
template<size_t N>
struct FixedString
{
    char value[N] = {};

    constexpr FixedString(const char (&text)[N])
    {
        std::copy_n(text, N, value);
    }

    [[nodiscard]] constexpr std::string_view view() const { return std::string_view(value, N - 1); }
};

template<size_t N>
FixedString(const char (&)[N]) -> FixedString<N>;

} // namespace oryx
