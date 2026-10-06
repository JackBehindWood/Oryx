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
    return type.defines_for(permutation);
}

ShaderTypeRegistrar::ShaderTypeRegistrar(const ShaderType& type)
{
    types().push_back(type);
}

} // namespace oryx
