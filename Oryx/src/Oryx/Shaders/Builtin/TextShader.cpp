#include "oxpch.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

namespace oryx
{

namespace
{

constexpr const char* SOURCE = "/Oryx/Builtin/Text.msl";

} // namespace

std::vector<ShaderDefine> TextPS::defines_for(uint32_t permutation)
{
    return QuadPS::defines_for(permutation);
}

OX_REGISTER_SHADER(TextVS, SOURCE, "text_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(TextPS, SOURCE, "text_ps", ShaderStage::Pixel)

} // namespace oryx
