#include "oxpch.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

namespace oryx
{

namespace
{

constexpr const char* SOURCE = "/Oryx/Builtin/Solid.msl";

} // namespace

OX_REGISTER_SHADER(SolidVS, SOURCE, "solid_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(SolidPS, SOURCE, "solid_ps", ShaderStage::Pixel)

} // namespace oryx
