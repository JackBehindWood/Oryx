#pragma once

#include "Oryx/Shaders/ShaderCompiler.h"

namespace oryx
{

// Base of the engine's own shaders (`class QuadPS : public BuiltinShader<PixelShader>`); register one with OX_REGISTER_SHADER.
// A subclass shadows defines_for/should_compile to add permutations.
template<typename StageShader>
class BuiltinShader : public StageShader
{
public:
    using StageShader::StageShader;

    static std::vector<ShaderDefine> defines_for(uint32_t) { return {}; }
    static bool should_compile(uint32_t permutation) { return permutation == 0; }
};

} // namespace oryx
