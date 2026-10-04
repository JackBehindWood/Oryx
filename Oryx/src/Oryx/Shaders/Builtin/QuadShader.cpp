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

struct QuadIn
{
    float3 position [[attribute(0)]];
    float4 colour [[attribute(1)]];
    float2 uv [[attribute(2)]];
    float tex_index [[attribute(3)]];
};

struct QuadOut
{
    float4 position [[position]];
    float4 colour;
    float2 uv;
    float tex_index;
};

vertex QuadOut quad_vs(QuadIn in [[stage_in]], constant Frame& frame [[buffer(0)]])
{
    QuadOut out;
    out.position = frame.view_projection * float4(in.position, 1.0);
    out.colour = in.colour;
    out.uv = in.uv;
    out.tex_index = in.tex_index;
    return out;
}

fragment float4 quad_ps(QuadOut in [[stage_in]], array<texture2d<float>, MAX_TEXTURES> textures [[texture(0)]], sampler smp [[sampler(0)]])
{
    uint index = min(uint(in.tex_index + 0.5), uint(MAX_TEXTURES - 1));
    return in.colour * textures[index].sample(smp, in.uv);
}
)msl";

} // namespace

std::vector<ShaderDefine> QuadPS::defines_for(uint32_t permutation)
{
    return { { "MAX_TEXTURES", std::to_string(texture_count(permutation)) } };
}

OX_REGISTER_SHADER(QuadVS, SOURCE, "quad_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(QuadPS, SOURCE, "quad_ps", ShaderStage::Pixel)

VertexLayout quad_vertex_layout()
{
    return { { { 0, RHIVertexFormat::Float3, 0, 0 }, { 1, RHIVertexFormat::Float4, 12, 0 }, { 2, RHIVertexFormat::Float2, 28, 0 }, { 3, RHIVertexFormat::Float, 36, 0 } }, 40 };
}

} // namespace oryx
