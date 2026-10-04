#pragma once

#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Graphics/RHI/RHIResource.h"

namespace oryx
{

inline constexpr uint32_t RHI_SHADER_STAGE_COUNT = static_cast<uint32_t>(RHIShaderStage::TessEval) + 1;

// code is backend-defined (MSL text for Metal); code and entry_point are only read during creation.
struct RHIShaderDesc
{
    RHIShaderStage stage = RHIShaderStage::Vertex;
    const char* entry_point = "main";
    const uint8_t* code = nullptr;
    uint32_t code_size = 0;
};

class RHIShader : public RHIResource
{
public:
    [[nodiscard]] virtual inline RHIShaderStage stage() const = 0;

protected:
    RHIShader() = default;
};

class RHIVertexShader : public RHIShader
{
public:
    [[nodiscard]] inline RHIShaderStage stage() const final { return RHIShaderStage::Vertex; }

protected:
    explicit RHIVertexShader(const RHIShaderDesc&) {}
};

class RHIPixelShader : public RHIShader
{
public:
    [[nodiscard]] inline RHIShaderStage stage() const final { return RHIShaderStage::Pixel; }

protected:
    explicit RHIPixelShader(const RHIShaderDesc&) {}
};

using RHIShaderPtr = Ref<RHIShader>;
using RHIVertexShaderPtr = Ref<RHIVertexShader>;
using RHIPixelShaderPtr = Ref<RHIPixelShader>;

} // namespace oryx
