#include "Oryx/Common.slang"

Texture2D<float4> textures[OX_MAX_TEXTURES];
SamplerState smp;

struct QuadIn
{
    float3 position : POSITION;
    float4 colour : COLOR;
    float2 uv : TEXCOORD0;
    float tex_index : TEXCOORD1;
};

struct QuadOut
{
    float4 position : SV_Position;
    float4 colour : COLOR;
    float2 uv : TEXCOORD0;
    float tex_index : TEXCOORD1;
};

[shader("vertex")]
QuadOut quad_vs(QuadIn in_vertex)
{
    QuadOut out_vertex;
    out_vertex.position = mul(frame.view_projection, float4(in_vertex.position, 1.0));
    out_vertex.colour = in_vertex.colour;
    out_vertex.uv = in_vertex.uv;
    out_vertex.tex_index = in_vertex.tex_index;
    return out_vertex;
}

[shader("fragment")]
float4 quad_ps(QuadOut in_pixel) : SV_Target
{
    uint index = min(uint(in_pixel.tex_index + 0.5), uint(OX_MAX_TEXTURES - 1));
    return in_pixel.colour * textures[index].Sample(smp, in_pixel.uv);
}
