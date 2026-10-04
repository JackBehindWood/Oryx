#pragma once

#include "Oryx/Shaders/ShaderCompiler.h"

namespace oryx
{

// FNV-1a over language, text, included texts, entry, stage, defines sorted by name and the compiler id/version; never std::hash.
[[nodiscard]] ShaderHash hash_shader_input(const ShaderCompilerInput& input, const char* compiler_id, uint32_t compiler_version);

} // namespace oryx
