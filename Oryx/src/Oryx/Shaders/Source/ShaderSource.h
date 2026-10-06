#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

enum class ShaderLanguage : uint8_t
{
    MSL,
    Slang
};

inline constexpr uint32_t SHADER_LANGUAGE_COUNT = static_cast<uint32_t>(ShaderLanguage::Slang) + 1;

[[nodiscard]] const char* shader_language_name(ShaderLanguage language);

// name only labels diagnostics; it is not hashed.
struct ShaderSource
{
    std::string name;
    ShaderLanguage language = ShaderLanguage::MSL;
    std::string text;
};

} // namespace oryx
