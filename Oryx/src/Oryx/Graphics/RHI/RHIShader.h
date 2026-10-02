#pragma once

#include "Oryx/Graphics/RHI/RHIResource.h"

namespace oryx
{

// Compute and tessellation stages are reserved; no backend creates them yet.
enum class ShaderStage : uint8_t
{
    Vertex,
    Pixel,
    Compute,
    TessControl,
    TessEval
};

inline constexpr uint32_t SHADER_STAGE_COUNT = static_cast<uint32_t>(ShaderStage::TessEval) + 1;

// code is backend-defined (MSL text for Metal); code and entry_point are only read during creation.
struct RHIShaderDesc
{
    ShaderStage stage = ShaderStage::Vertex;
    const char* entry_point = "main";
    const uint8_t* code = nullptr;
    uint32_t code_size = 0;
};

class RHIShader : public RHIResource
{
public:
    [[nodiscard]] ShaderStage stage() const { return m_stage; }

protected:
    explicit RHIShader(const RHIShaderDesc& desc)
        : RHIResource()
        , m_stage(desc.stage)
    {
    }

private:
    ShaderStage m_stage;
};

class RHIVertexShader : public RHIShader
{
protected:
    explicit RHIVertexShader(const RHIShaderDesc& desc)
        : RHIShader(desc)
    {
    }
};

class RHIPixelShader : public RHIShader
{
protected:
    explicit RHIPixelShader(const RHIShaderDesc& desc)
        : RHIShader(desc)
    {
    }
};

using RHIShaderPtr = Ref<RHIShader>;
using RHIVertexShaderPtr = Ref<RHIVertexShader>;
using RHIPixelShaderPtr = Ref<RHIPixelShader>;

} // namespace oryx
