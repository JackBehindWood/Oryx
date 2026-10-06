#pragma once

#include "Oryx/Shaders/Compiler/ShaderCompiler.h"

namespace oryx
{

// FNV-1a over language, text, included texts, entry, stage, target, defines sorted by name and the compiler id/version; never std::hash.
// Includes are found through the language's compiler (dependencies()), looked up in input.includes first and then the include registry.
[[nodiscard]] ShaderHash hash_shader_input(const ShaderCompilerInput& input, const IShaderCompiler& compiler);
// Same, with an explicit identity for the compiler of input.source.language.
[[nodiscard]] ShaderHash hash_shader_input(const ShaderCompilerInput& input, const char* compiler_id, uint32_t compiler_version);

} // namespace oryx
