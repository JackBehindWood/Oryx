#pragma once

#include "Oryx/Core/CommandLine.h"
#include "Oryx/Shaders/Cache/ShaderCook.h"

namespace oryx
{

// True when --cook-shaders was given. The option is declared by this module's own command-line contributor.
[[nodiscard]] bool shader_cook_requested(const ParsedArgs& args);

// Cooks every registered shader from the configured sources (shaders.root, or the embedded copies) into the application's compiled
// store, so a Dist build can run with shaders.source_mode: cooked. Headless; throws Error naming the shader that failed.
ShaderCookResult run_shader_cook();

} // namespace oryx
