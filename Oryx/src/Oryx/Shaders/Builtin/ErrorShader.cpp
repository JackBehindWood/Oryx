#include "oxpch.h"
#include "Oryx/Shaders/Builtin/ErrorShader.h"

namespace oryx
{

namespace
{

constexpr const char* ERROR_PS_SOURCE = R"(#include <metal_stdlib>
using namespace metal;

fragment float4 error_ps()
{
    return float4(1.0, 0.0, 1.0, 1.0);
}
)";

} // namespace

ShaderCompilerInput error_pixel_shader_input()
{
    ShaderCompilerInput input;
    input.source = { "/Oryx/Builtin/Error.msl", ShaderLanguage::MSL, ERROR_PS_SOURCE };
    input.stage = ShaderStage::Pixel;
    input.entry_point = "error_ps";
    return input;
}

} // namespace oryx
