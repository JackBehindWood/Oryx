#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

// In-memory include sources, filled by self-registering statics; no filesystem access.
void register_shader_include(const std::string& name, std::string text);
// Returns nullptr when the name is unknown.
[[nodiscard]] const std::string* find_shader_include(const std::string& name);

// Names of every quoted `#include "name"` reachable from `text`, sorted and unique; unknown names are listed but not followed.
[[nodiscard]] std::vector<std::string> shader_include_closure(const std::string& text);

struct ShaderIncludeRegistrar
{
    ShaderIncludeRegistrar(const char* name, const char* text) { register_shader_include(name, text); }
};

} // namespace oryx
