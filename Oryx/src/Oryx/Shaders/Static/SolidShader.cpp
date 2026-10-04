#include "oxpch.h"
#include "Oryx/Shaders/Static/StaticShaders.h"

namespace oryx
{

namespace
{

constexpr const char* SOURCE = R"msl(
#include "Oryx/Common.msl"

struct SolidIn
{
    float3 position [[attribute(0)]];
    float4 colour [[attribute(1)]];
};

struct SolidOut
{
    float4 position [[position]];
    float4 colour;
};

vertex SolidOut solid_vs(SolidIn in [[stage_in]], constant Frame& frame [[buffer(0)]])
{
    SolidOut out;
    out.position = frame.view_projection * float4(in.position, 1.0);
    out.colour = in.colour;
    return out;
}

fragment float4 solid_ps(SolidOut in [[stage_in]])
{
    return in.colour;
}
)msl";

} // namespace

OX_REGISTER_SHADER(SolidVS, SOURCE, "solid_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(SolidPS, SOURCE, "solid_ps", ShaderStage::Pixel)

VertexLayout solid_vertex_layout()
{
    return { { { 0, RHIVertexFormat::Float3, 0, 0 }, { 1, RHIVertexFormat::Float4, 12, 0 } }, 28 };
}

} // namespace oryx
