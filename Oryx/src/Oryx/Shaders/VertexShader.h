#pragma once

#include "Oryx/Graphics/RHI/IRHI.h"
#include "Oryx/Shaders/Shader.h"

namespace oryx
{

class VertexShader : public Shader
{
public:
    VertexShader(const ShaderCompilerOutput& output, uint32_t permutation, RHIVertexShaderPtr rhi)
        : Shader(output, permutation)
        , m_rhi(std::move(rhi))
    {
    }

    [[nodiscard]] const RHIVertexShaderPtr& rhi() const { return m_rhi; }

    static RHIVertexShaderPtr create_rhi_shader(IRHI& rhi, const ShaderCompilerOutput& output, const char* entry_point)
    {
        return rhi.create_vertex_shader({ .stage = RHIShaderStage::Vertex, .entry_point = entry_point, .code = output.binary.data(), .code_size = static_cast<uint32_t>(output.binary.size()) });
    }

private:
    RHIVertexShaderPtr m_rhi;
};

using VertexShaderPtr = Ref<VertexShader>;

} // namespace oryx
