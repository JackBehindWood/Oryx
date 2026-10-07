#include "oxpch.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

namespace oryx
{

namespace
{

constexpr const char* SOLID_SOURCE = "/Oryx/Builtin/Solid.slang";
constexpr const char* QUAD_SOURCE = "/Oryx/Builtin/Quad.slang";
constexpr const char* CIRCLE_SOURCE = "/Oryx/Builtin/Circle.slang";
constexpr const char* TEXT_SOURCE = "/Oryx/Builtin/Text.slang";
constexpr const char* UI_SOURCE = "/Oryx/Builtin/Ui.slang";

} // namespace

OX_REGISTER_SHADER(SolidVS, SOLID_SOURCE, "solid_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(SolidPS, SOLID_SOURCE, "solid_ps", ShaderStage::Pixel)
OX_REGISTER_SHADER(QuadVS, QUAD_SOURCE, "quad_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(QuadPS, QUAD_SOURCE, "quad_ps", ShaderStage::Pixel)
OX_REGISTER_SHADER(CircleVS, CIRCLE_SOURCE, "circle_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(CirclePS, CIRCLE_SOURCE, "circle_ps", ShaderStage::Pixel)
OX_REGISTER_SHADER(TextVS, TEXT_SOURCE, "text_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(TextPS, TEXT_SOURCE, "text_ps", ShaderStage::Pixel)
OX_REGISTER_SHADER(UiVS, UI_SOURCE, "ui_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(UiPS, UI_SOURCE, "ui_ps", ShaderStage::Pixel)

} // namespace oryx
