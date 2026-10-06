#pragma once

#include "Oryx/Shaders/Compiler/ShaderCompiler.h"

namespace oryx
{

// The pixel shader ShaderLibrary substitutes for one that failed its first compile: solid magenta, no inputs and no resources, authored in MSL so it needs no slangc.
[[nodiscard]] ShaderCompilerInput error_pixel_shader_input();

} // namespace oryx
