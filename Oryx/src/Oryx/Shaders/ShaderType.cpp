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

ShaderTypeRegistrar::ShaderTypeRegistrar(const ShaderType& type)
{
    types().push_back(type);
}

} // namespace oryx
