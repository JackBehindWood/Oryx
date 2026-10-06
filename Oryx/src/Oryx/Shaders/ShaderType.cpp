#include "oxpch.h"
#include "Oryx/Shaders/ShaderType.h"

namespace oryx
{

namespace
{

std::vector<ShaderType>& types()
{
    static std::vector<ShaderType> list;
    return list;
}

} // namespace

const std::vector<ShaderType>& registered_shader_types()
{
    return types();
}

std::vector<ShaderDefine> shader_defines(const ShaderType& type, uint32_t permutation)
{
    std::vector<ShaderDefine> defines = { { "OX_MAX_TEXTURES", std::to_string(RHI_MAX_TEXTURE_BINDINGS) } };
    for (ShaderDefine& own : type.defines_for(permutation))
    {
        const std::vector<ShaderDefine>::iterator existing = std::find_if(defines.begin(), defines.end(), [&](const ShaderDefine& define) { return define.name == own.name; });
        if (existing != defines.end())
        {
            existing->value = std::move(own.value);
        }
        else
        {
            defines.push_back(std::move(own));
        }
    }
    return defines;
}

ShaderTypeRegistrar::ShaderTypeRegistrar(const ShaderType& type)
{
    types().push_back(type);
}

} // namespace oryx
