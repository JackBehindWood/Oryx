#include "Oryx/Common.slang"

Texture2D<float4> textures[OX_MAX_TEXTURES];
SamplerState smp;

// Mirrors UiMode and UI_SHAPE_MARGIN in Vertex2D.h.
static const uint MODE_TEXTURE = 0;
static const uint MODE_COVERAGE = 1;
static const uint MODE_DISTANCE = 2;
static const uint MODE_FILL = 3;
static const float SHAPE_MARGIN = 1.0;

struct UiIn
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 colour : COLOR;
    float4 control : TEXCOORD1;
    float4 radii : TEXCOORD2;
};

struct UiOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 colour : COLOR;
    float4 control : TEXCOORD1;
    float4 radii : TEXCOORD2;
    nointerpolation float2 half_size : TEXCOORD3;
};

[shader("vertex")]
UiOut ui_vs(UiIn in_vertex)
{
    UiOut out_vertex;
    out_vertex.position = mul(frame.view_projection, float4(in_vertex.position, 0.0, 1.0));
    out_vertex.uv = in_vertex.uv;
    out_vertex.colour = in_vertex.colour;
    out_vertex.control = in_vertex.control;
    out_vertex.radii = in_vertex.radii;
    out_vertex.half_size = abs(in_vertex.uv) - SHAPE_MARGIN;
    return out_vertex;
}

// Signed distance to a box of half-extent `half_size` whose corner radii are (top-left, top-right, bottom-right, bottom-left) with y up.
float rounded_box(float2 p, float2 half_size, float4 radii)
{
    float radius = p.x > 0.0 ? (p.y > 0.0 ? radii.y : radii.z) : (p.y > 0.0 ? radii.x : radii.w);
    float2 q = abs(p) - half_size + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

[shader("fragment")]
float4 ui_ps(UiOut in_pixel) : SV_Target
{
    uint index = min(uint(in_pixel.control.x * 255.0 + 0.5), uint(OX_MAX_TEXTURES - 1));
    uint mode = uint(in_pixel.control.y * 255.0 + 0.5);
    float4 texel = textures[index].Sample(smp, in_pixel.uv);

    float min_half = min(in_pixel.half_size.x, in_pixel.half_size.y);
    float thickness = in_pixel.control.z * min_half;
    float4 radii = in_pixel.radii * min_half;
    float outer = rounded_box(in_pixel.uv, in_pixel.half_size, radii);
    float inner = rounded_box(in_pixel.uv, in_pixel.half_size - thickness, max(radii - thickness, 0.0));
    float outer_width = max(fwidth(outer), 0.0001);
    float inner_width = max(fwidth(inner), 0.0001);
    float outer_cover = saturate(0.5 - outer / outer_width);
    float inner_cover = saturate(0.5 - inner / inner_width);
    float width = max(fwidth(texel.r), 0.0001);
    float distance_cover = smoothstep(0.5 - width, 0.5 + width, texel.r);

    if (mode == MODE_TEXTURE)
    {
        return in_pixel.colour * texel;
    }
    if (mode == MODE_COVERAGE)
    {
        return float4(in_pixel.colour.rgb, in_pixel.colour.a * texel.r);
    }
    if (mode == MODE_DISTANCE)
    {
        return float4(in_pixel.colour.rgb, in_pixel.colour.a * distance_cover);
    }
    float coverage = mode == MODE_FILL ? outer_cover : outer_cover * (1.0 - inner_cover);
    return float4(in_pixel.colour.rgb, in_pixel.colour.a * coverage);
}
