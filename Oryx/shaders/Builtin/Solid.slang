#include "Oryx/Common.slang"

struct SolidIn
{
    float3 position : POSITION;
    float4 colour : COLOR;
};

struct SolidOut
{
    float4 position : SV_Position;
    float4 colour : COLOR;
};

[shader("vertex")]
SolidOut solid_vs(SolidIn in_vertex)
{
    SolidOut out_vertex;
    out_vertex.position = mul(frame.view_projection, float4(in_vertex.position, 1.0));
    out_vertex.colour = in_vertex.colour;
    return out_vertex;
}

[shader("fragment")]
float4 solid_ps(SolidOut in_pixel) : SV_Target
{
    return in_pixel.colour;
}
