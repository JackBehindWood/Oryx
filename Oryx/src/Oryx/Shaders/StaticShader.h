#pragma once

#include "Oryx/Shaders/Compiler/ShaderCompiler.h"

namespace oryx
{

// Base of a shader whose source is fixed in code, the engine's own or an application's (`class QuadPS : public StaticShader<PixelShader>`);
// register one with OX_REGISTER_SHADER and ShaderLibrary::compile_all builds it. A subclass shadows defines_for/should_compile to add permutations.
template<typename StageShader>
class StaticShader : public StageShader
{
public:
    using StageShader::StageShader;

    static std::vector<ShaderDefine> defines_for(uint32_t) { return {}; }
    static bool should_compile(uint32_t permutation) { return permutation == 0; }
};

} // namespace oryx
