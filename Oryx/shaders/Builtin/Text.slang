#include "Oryx/Common.slang"

Texture2D<float4> textures[OX_MAX_TEXTURES];
SamplerState smp;

struct TextIn
{
    float3 position : POSITION;
    float4 colour : COLOR;
    float2 uv : TEXCOORD0;
    float tex_index : TEXCOORD1;
    float px_range : TEXCOORD2;
};

struct TextOut
{
    float4 position : SV_Position;
    float4 colour : COLOR;
    float2 uv : TEXCOORD0;
    float tex_index : TEXCOORD1;
    float px_range : TEXCOORD2;
};

[shader("vertex")]
TextOut text_vs(TextIn in_vertex)
{
    TextOut out_vertex;
    out_vertex.position = mul(frame.view_projection, float4(in_vertex.position, 1.0));
    out_vertex.colour = in_vertex.colour;
    out_vertex.uv = in_vertex.uv;
    out_vertex.tex_index = in_vertex.tex_index;
    out_vertex.px_range = in_vertex.px_range;
    return out_vertex;
}

[shader("fragment")]
float4 text_ps(TextOut in_pixel) : SV_Target
{
    uint index = min(uint(in_pixel.tex_index + 0.5), uint(OX_MAX_TEXTURES - 1));
    float value = textures[index].Sample(smp, in_pixel.uv).r;
    float coverage = value;
    if (in_pixel.px_range > 0.0)
    {
        float width = max(fwidth(value), 0.0001);
        coverage = smoothstep(0.5 - width, 0.5 + width, value);
    }
    return float4(in_pixel.colour.rgb, in_pixel.colour.a * coverage);
}
