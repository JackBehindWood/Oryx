#include "oxpch.h"
#include "Oryx/Shaders/Shader.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

const ShaderBinding& Shader::binding(std::string_view name) const
{
    const ShaderBinding* found = find_binding(m_reflection.parameters, name);
    if (found == nullptr)
    {
        throw Error("shader '" + m_reflection.entry_point + "' has no binding named '" + std::string(name) + "'");
    }
    return *found;
}

} // namespace oryx
