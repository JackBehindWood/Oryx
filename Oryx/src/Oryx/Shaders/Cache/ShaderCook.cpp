#include "oxpch.h"
#include "Oryx/Shaders/Cache/ShaderCook.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Shaders/ShaderLibrary.h"
#include "Oryx/Shaders/Cache/ShaderMap.h"

namespace oryx
{

ShaderCookResult cook_shaders(ShaderCache& cache, const IShaderSourceProvider& sources, const IShaderBinaryStore& store)
{
    ShaderMap map;
    map.id = shader_map_id(registered_shader_types());
    for (const ShaderType& type : registered_shader_types())
    {
        for (uint32_t permutation = 0; permutation < SHADER_MAX_PERMUTATIONS; ++permutation)
        {
            if (!type.should_compile(permutation))
            {
                continue;
            }
            try
            {
                ShaderCompilerInput input = load_shader_input(type, sources);
                input.defines = shader_defines(type, permutation);
                const ShaderCompilerOutput& output = cache.get_or_compile(input);
                write_shader_output(store, output.hash, output);
                map.entries.push_back({ type.name, permutation, output.hash });
            }
            catch (const Error& error)
            {
                throw Error(std::string(type.name) + " (permutation " + std::to_string(permutation) + "): " + error.what());
            }
        }
    }
    write_shader_map(store, map);
    return { static_cast<uint32_t>(map.entries.size()), map.id };
}

} // namespace oryx
