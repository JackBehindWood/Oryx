#pragma once

#include "Oryx/Memory/RefCounted.h"
#include "Oryx/Shaders/ShaderCompiler.h"

namespace oryx
{

// One compiled stage: its reflection, content hash and permutation. Typed stages (VertexShader, PixelShader) add the RHI object.
class Shader : public RefCounted
{
public:
    [[nodiscard]] ShaderStage stage() const { return m_reflection.stage; }
    [[nodiscard]] const ShaderReflection& reflection() const { return m_reflection; }
    [[nodiscard]] ShaderHash hash() const { return m_hash; }
    [[nodiscard]] uint32_t permutation() const { return m_permutation; }
    // Throws Error for a name the shader does not declare.
    [[nodiscard]] const ShaderBinding& binding(std::string_view name) const;

protected:
    Shader(const ShaderCompilerOutput& output, uint32_t permutation)
        : m_reflection(output.reflection)
        , m_hash(output.hash)
        , m_permutation(permutation)
    {
    }

private:
    ShaderReflection m_reflection;
    ShaderHash m_hash;
    uint32_t m_permutation;
};

using ShaderPtr = Ref<Shader>;

} // namespace oryx
