#include "oxpch.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

namespace oryx
{

namespace
{

constexpr const char* SOURCE = "/Oryx/Builtin/Circle.slang";

} // namespace

OX_REGISTER_SHADER(CircleVS, SOURCE, "circle_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(CirclePS, SOURCE, "circle_ps", ShaderStage::Pixel)

} // namespace oryx
