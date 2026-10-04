#pragma once

#include "Oryx.h"
#include "NullRHI.h"

namespace oryx::test
{

inline const char* SAMPLE_MSL = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexIn
{
    float3 position [[attribute(0)]];
    float4 colour [[attribute(1)]];
    float2 uv [[attribute(2)]];
};

struct VertexOut
{
    float4 position [[position]];
    float4 colour;
    float2 uv;
};

struct Frame
{
    float4x4 view_projection;
    float3 tint;
    float scale;
};

vertex VertexOut vs_main(VertexIn in [[stage_in]], constant Frame& frame [[buffer(0)]])
{
    VertexOut out;
    out.position = frame.view_projection * float4(in.position, 1.0);
    out.colour = in.colour;
    out.uv = in.uv;
    return out;
}

fragment float4 ps_main(VertexOut in [[stage_in]], array<texture2d<float>, 4> textures [[texture(0)]], sampler smp [[sampler(0)]])
{
    return in.colour * textures[0].sample(smp, in.uv);
}
)";

inline ShaderCompilerInput make_input(ShaderStage stage, const std::string& entry, const std::string& text, std::vector<ShaderDefine> defines = {})
{
    ShaderCompilerInput input;
    input.source = { "test.msl", ShaderLanguage::MSL, text };
    input.stage = stage;
    input.entry_point = entry;
    input.defines = std::move(defines);
    return input;
}

inline ShaderCompilerOutput compile_msl(ShaderStage stage, const std::string& entry, const std::string& text, std::vector<ShaderDefine> defines = {})
{
    return MslShaderCompiler().compile(make_input(stage, entry, text, std::move(defines)));
}

inline std::string compile_error(ShaderStage stage, const std::string& entry, const std::string& text)
{
    try
    {
        compile_msl(stage, entry, text);
    }
    catch (const Error& error)
    {
        return error.what();
    }
    return {};
}

} // namespace oryx::test

namespace oryx::test
{

template<typename T>
const ShaderType& shader_type_of()
{
    for (const ShaderType& type : registered_shader_types())
    {
        if (type.type == std::type_index(typeid(T)))
        {
            return type;
        }
    }
    throw Error("shader type is not registered");
}

inline Ref<VertexShader> make_vertex_shader(IRHI& rhi, ShaderCache& cache, const std::string& text, const std::string& entry)
{
    const ShaderCompilerOutput& output = cache.get_or_compile(make_input(ShaderStage::Vertex, entry, text));
    return make_ref<VertexShader>(output, 0, VertexShader::create_rhi_shader(rhi, output, entry.c_str()));
}

inline Ref<PixelShader> make_pixel_shader(IRHI& rhi, ShaderCache& cache, const std::string& text, const std::string& entry)
{
    const ShaderCompilerOutput& output = cache.get_or_compile(make_input(ShaderStage::Pixel, entry, text));
    return make_ref<PixelShader>(output, 0, PixelShader::create_rhi_shader(rhi, output, entry.c_str()));
}

} // namespace oryx::test
