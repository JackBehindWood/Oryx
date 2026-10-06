#include "Oryx/Common.slang"

struct CircleIn
{
    float3 position : POSITION;
    float4 colour : COLOR;
    float2 local_position : TEXCOORD0;
    float thickness : TEXCOORD1;
    float fade : TEXCOORD2;
};

struct CircleOut
{
    float4 position : SV_Position;
    float4 colour : COLOR;
    float2 local_position : TEXCOORD0;
    float thickness : TEXCOORD1;
    float fade : TEXCOORD2;
};

[shader("vertex")]
CircleOut circle_vs(CircleIn in_vertex)
{
    CircleOut out_vertex;
    out_vertex.position = mul(frame.view_projection, float4(in_vertex.position, 1.0));
    out_vertex.colour = in_vertex.colour;
    out_vertex.local_position = in_vertex.local_position;
    out_vertex.thickness = in_vertex.thickness;
    out_vertex.fade = in_vertex.fade;
    return out_vertex;
}

[shader("fragment")]
float4 circle_ps(CircleOut in_pixel) : SV_Target
{
    float distance = 1.0 - length(in_pixel.local_position);
    float coverage = smoothstep(0.0, in_pixel.fade, distance) * smoothstep(in_pixel.thickness + in_pixel.fade, in_pixel.thickness, distance);
    return float4(in_pixel.colour.rgb, in_pixel.colour.a * coverage);
}
