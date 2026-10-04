#include "oxpch.h"
#include "Oryx/Shaders/ShaderLibrary.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

void ShaderLibrary::compile(IRHI& rhi, ShaderCache& cache, const ShaderType& type)
{
    for (uint32_t permutation = 0; permutation < SHADER_MAX_PERMUTATIONS; ++permutation)
    {
        if (!type.should_compile(permutation))
        {
            continue;
        }
        try
        {
            ShaderCompilerInput input;
            input.source = { type.name, ShaderLanguage::MSL, type.source };
            input.stage = type.stage;
            input.entry_point = type.entry_point;
            input.defines = type.defines_for(permutation);
            const ShaderCompilerOutput& output = cache.get_or_compile(input);
            m_shaders[{ type.type, permutation }] = type.create(rhi, output, permutation);
        }
        catch (const Error& error)
        {
            throw Error(std::string(type.name) + " (permutation " + std::to_string(permutation) + "): " + error.what());
        }
    }
}

void ShaderLibrary::compile_all(IRHI& rhi, ShaderCache& cache)
{
    for (const ShaderType& type : registered_shader_types())
    {
        compile(rhi, cache, type);
    }
}

const Ref<Shader>& ShaderLibrary::find(std::type_index type, uint32_t permutation) const
{
    const std::map<std::pair<std::type_index, uint32_t>, Ref<Shader>>::const_iterator it = m_shaders.find({ type, permutation });
    if (it == m_shaders.end())
    {
        throw Error(std::string("shader '") + type.name() + "' permutation " + std::to_string(permutation) + " is not in the library");
    }
    return it->second;
}

} // namespace oryx
