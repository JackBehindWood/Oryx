#pragma once

#include "Oryx/Core/Error.h"

namespace oryx
{

class ScriptError : public Error
{
public:
    explicit ScriptError(const std::string& message, std::string traceback = "")
        : Error(message, std::move(traceback))
    {
    }

    [[nodiscard]] const char* category() const noexcept override { return "script"; }

    [[nodiscard]] const std::string& traceback() const { return detail(); }
};

class IllegalActionError : public ScriptError
{
public:
    explicit IllegalActionError(const std::string& message)
        : ScriptError(message)
    {
    }

    [[nodiscard]] const char* category() const noexcept override { return "illegal_action"; }
};

} // namespace oryx
