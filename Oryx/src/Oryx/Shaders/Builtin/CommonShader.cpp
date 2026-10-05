#include "oxpch.h"
#include "Oryx/Shaders/ShaderInclude.h"

namespace oryx
{

namespace
{

const ShaderIncludeRegistrar common("Oryx/Common.msl", R"msl(
#include <metal_stdlib>
using namespace metal;

struct Frame
{
    float4x4 view_projection;
};
)msl");

} // namespace

} // namespace oryx
