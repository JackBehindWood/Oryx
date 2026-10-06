#pragma once

#include "Oryx/Graphics/RHI/IRHI.h"
#include "Oryx/Shaders/Shader.h"

namespace oryx
{

class PixelShader : public Shader
{
public:
    PixelShader(const ShaderCompilerOutput& output, uint32_t permutation, RHIPixelShaderPtr rhi)
        : Shader(output, permutation)
        , m_rhi(std::move(rhi))
    {
    }

    [[nodiscard]] const RHIPixelShaderPtr& rhi() const { return m_rhi; }

    static RHIPixelShaderPtr create_rhi_shader(IRHI& rhi, const ShaderCompilerOutput& output, const char* entry_point)
    {
        return rhi.create_pixel_shader({ .stage = RHIShaderStage::Pixel, .entry_point = entry_point, .code = output.binary.data(), .code_size = static_cast<uint32_t>(output.binary.size()) });
    }

private:
    RHIPixelShaderPtr m_rhi;
};

using PixelShaderPtr = Ref<PixelShader>;

} // namespace oryx
