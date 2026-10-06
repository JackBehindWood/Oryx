#pragma once

#include "Oryx/Shaders/Compiler/ShaderCompiler.h"

namespace oryx
{

// The persisted form of a compiled shader: format version, binary format, origin language, compiler identity, binary, diagnostics and the
// full reflection. Nothing in it is specific to one compiler, so any IShaderCompiler's output round-trips.
[[nodiscard]] std::vector<uint8_t> serialise_shader_output(const ShaderCompilerOutput& output);
// False for truncated, oversized or foreign data; `out` is untouched then.
[[nodiscard]] bool deserialise_shader_output(const uint8_t* data, size_t size, ShaderCompilerOutput& out);

} // namespace oryx
