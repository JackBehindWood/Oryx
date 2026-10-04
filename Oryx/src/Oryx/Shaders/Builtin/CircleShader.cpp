#include "oxpch.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

namespace oryx
{

namespace
{

constexpr const char* SOURCE = R"msl(
#include "Oryx/Common.msl"

struct CircleIn
{
    float3 position [[attribute(0)]];
    float4 colour [[attribute(1)]];
    float2 local_position [[attribute(2)]];
    float thickness [[attribute(3)]];
    float fade [[attribute(4)]];
};

struct CircleOut
{
    float4 position [[position]];
    float4 colour;
    float2 local_position;
    float thickness;
    float fade;
};

vertex CircleOut circle_vs(CircleIn in [[stage_in]], constant Frame& frame [[buffer(0)]])
{
    CircleOut out;
    out.position = frame.view_projection * float4(in.position, 1.0);
    out.colour = in.colour;
    out.local_position = in.local_position;
    out.thickness = in.thickness;
    out.fade = in.fade;
    return out;
}

fragment float4 circle_ps(CircleOut in [[stage_in]])
{
    float distance = 1.0 - length(in.local_position);
    float coverage = smoothstep(0.0, in.fade, distance) * smoothstep(in.thickness + in.fade, in.thickness, distance);
    return float4(in.colour.rgb, in.colour.a * coverage);
}
)msl";

} // namespace

OX_REGISTER_SHADER(CircleVS, SOURCE, "circle_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(CirclePS, SOURCE, "circle_ps", ShaderStage::Pixel)

VertexLayout circle_vertex_layout()
{
    return { { { 0, RHIVertexFormat::Float3, 0, 0 }, { 1, RHIVertexFormat::Float4, 12, 0 }, { 2, RHIVertexFormat::Float2, 28, 0 }, { 3, RHIVertexFormat::Float, 36, 0 }, { 4, RHIVertexFormat::Float, 40, 0 } }, 44 };
}

} // namespace oryx
