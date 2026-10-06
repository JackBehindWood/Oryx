#pragma once

#include "Oryx/Memory/RefCounted.h"
#include "Oryx/Shaders/Compiler/ShaderCompiler.h"

namespace oryx
{

class ShaderLibrary;

// One compiled stage: its reflection, content hash and permutation. Typed stages (VertexShader, PixelShader) add the RHI object.
class Shader : public RefCounted
{
public:
    [[nodiscard]] ShaderStage stage() const { return m_reflection.stage; }
    [[nodiscard]] const ShaderReflection& reflection() const { return m_reflection; }
    [[nodiscard]] ShaderHash hash() const { return m_hash; }
    [[nodiscard]] uint32_t permutation() const { return m_permutation; }
    // True for the magenta stand-in a library builds when a pixel shader fails its first compile.
    [[nodiscard]] bool is_fallback() const { return m_fallback; }
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
    friend class ShaderLibrary;

    ShaderReflection m_reflection;
    ShaderHash m_hash;
    uint32_t m_permutation;
    bool m_fallback = false;
};

using ShaderPtr = Ref<Shader>;

} // namespace oryx
