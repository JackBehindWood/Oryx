#pragma once

#include "Oryx/Shaders/ShaderPermutation.h"

namespace oryx
{

// Base of a shader whose source is fixed in code, the engine's own or an application's (`class QuadPS : public StaticShader<PixelShader, TextureArrayPermutations>`);
// register one with OX_REGISTER_SHADER and ShaderLibrary::compile_all builds it. The hooks default to `Domain`; a subclass shadows them for what a domain cannot express.
template<typename StageShader, typename Domain = SinglePermutation>
class StaticShader : public StageShader
{
public:
    using StageShader::StageShader;
    using PermutationDomain = Domain;

    static std::vector<ShaderDefine> defines_for(uint32_t permutation) { return Domain::defines_for(permutation); }
    static bool should_compile(uint32_t permutation) { return Domain::should_compile(permutation); }
};

} // namespace oryx
