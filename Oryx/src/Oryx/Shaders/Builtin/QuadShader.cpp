#include "oxpch.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

namespace oryx
{

namespace
{

constexpr const char* SOURCE = "/Oryx/Builtin/Quad.slang";

} // namespace

std::vector<ShaderDefine> QuadPS::defines_for(uint32_t permutation)
{
    return { { "OX_MAX_TEXTURES", std::to_string(texture_count(permutation)) } };
}

OX_REGISTER_SHADER(QuadVS, SOURCE, "quad_vs", ShaderStage::Vertex)
OX_REGISTER_SHADER(QuadPS, SOURCE, "quad_ps", ShaderStage::Pixel)

} // namespace oryx
