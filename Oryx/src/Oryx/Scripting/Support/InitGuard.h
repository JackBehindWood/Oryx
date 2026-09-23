#pragma once

#include "Oryx/Core/Application.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Core/FixedString.h"

namespace oryx
{

class NotInitialisedError : public Error
{
public:
    explicit NotInitialisedError(const std::string& message)
        : Error(message)
    {
    }

    [[nodiscard]] const char* category() const noexcept override { return "not_initialised"; }
};

[[noreturn]] void throw_not_initialised(std::string_view function_name);

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

// Yields a function pointer with the same signature that throws oryx::NotInitialisedError instead of running before oryx::init(); free functions only.
#define OX_GUARDED_FUNC(function, name) (&::oryx::Guarded<&function, ::oryx::FixedString(name)>::call)
