#pragma once

#include "Oryx/Core/Application.h"

namespace oryx
{

[[noreturn]] void throw_not_initialised(std::string_view function_name);

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

template<auto Function, FixedString Name, typename Signature = decltype(Function)>
struct Guarded;

template<auto Function, FixedString Name, typename Result, typename... Args>
struct Guarded<Function, Name, Result (*)(Args...)>
{
    static Result call(Args... args)
    {
        if (!is_initialised()) [[unlikely]]
        {
            throw_not_initialised(Name.view());
        }
        return Function(std::forward<Args>(args)...);
    }
};

} // namespace oryx

// Yields a function pointer with the same signature that throws oryx::Error instead of running before oryx::init(); free functions only.
#define OX_GUARDED_FUNC(function, name) (&::oryx::Guarded<&function, ::oryx::FixedString(name)>::call)
