#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

class Error : public std::runtime_error
{
public:
    explicit Error(const std::string& message, std::string detail = "")
        : std::runtime_error(message)
        , m_detail(std::move(detail))
    {
    }

    [[nodiscard]] virtual const char* category() const noexcept { return "error"; }
    [[nodiscard]] const std::string& detail() const { return m_detail; }

    void log() const;

private:
    std::string m_detail;
};

} // namespace oryx
