#include "oxpch.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

namespace oryx
{

namespace
{

constexpr const char* SOURCE = R"msl(
#include "Oryx/Common.msl"

#ifndef MAX_TEXTURES
#define MAX_TEXTURES 16
#endif

struct TextIn
{
    float3 position [[attribute(0)]];
    float4 colour [[attribute(1)]];
    float2 uv [[attribute(2)]];
    float tex_index [[attribute(3)]];
    float px_range [[attribute(4)]];
};

struct TextOut
{
    float4 position [[position]];
    float4 colour;
    float2 uv;
    float tex_index;
    float px_range;
};

vertex TextOut text_vs(TextIn in [[stage_in]], constant Frame& frame [[buffer(0)]])
{
    TextOut out;
    out.position = frame.view_projection * float4(in.position, 1.0);
    out.colour = in.colour;
    out.uv = in.uv;
    out.tex_index = in.tex_index;
    out.px_range = in.px_range;
    return out;
}

fragment float4 text_ps(TextOut in [[stage_in]], array<texture2d<float>, MAX_TEXTURES> textures [[texture(0)]], sampler smp [[sampler(0)]])
{
    uint index = min(uint(in.tex_index + 0.5), uint(MAX_TEXTURES - 1));
    float value = textures[index].sample(smp, in.uv).r;
    float coverage = value;
    if (in.px_range > 0.0)
    {
        float width = max(fwidth(value), 0.0001);
        coverage = smoothstep(0.5 - width, 0.5 + width, value);
    }
    return float4(in.colour.rgb, in.colour.a * coverage);
}
)msl";

} // namespace

std::vector<ShaderDefine> TextPS::defines_for(uint32_t permutation)
{
    return QuadPS::defines_for(permutation);
}

OX_REGISTER_SHADER(TextVS, SOURCE, "text_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(TextPS, SOURCE, "text_ps", ShaderStage::Pixel)

} // namespace oryx
