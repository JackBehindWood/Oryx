#include "oxpch.h"
#include "Oryx/Shaders/ShaderLibrary.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Shaders/Compiler/ShaderInclude.h"
#include "Oryx/Shaders/Cache/ShaderMap.h"

namespace oryx
{

ShaderCompilerInput load_shader_input(const ShaderType& type, const IShaderSourceProvider& sources)
{
    ShaderCompilerInput input;
    std::optional<ShaderSource> source = sources.load(type.source);
    if (!source)
    {
        throw Error(std::string("shader source '") + type.source + "' not found");
    }
    input.source = std::move(*source);
    input.source.name = type.source;
    input.stage = type.stage;
    input.entry_point = type.entry_point;

    const IShaderCompiler& compiler = shader_compiler_for(input.source.language);
    std::vector<std::string> pending = compiler.dependencies(input.source);
    while (!pending.empty())
    {
        const std::string name = pending.back();
        pending.pop_back();
        if (input.includes.count(name) != 0)
        {
            continue;
        }
        std::optional<ShaderSource> include = sources.load(shader_include_virtual_path(name));
        if (!include)
        {
            if (find_shader_include(name) == nullptr)
            {
                throw Error("include '" + name + "' not found");
            }
            continue;
        }
        for (const std::string& nested : compiler.dependencies(*include))
        {
            pending.push_back(nested);
        }
        input.includes[name] = std::move(include->text);
    }
    return input;
}

void ShaderLibrary::compile(IRHI& rhi, ShaderCache& cache, const ShaderType& type, const IShaderSourceProvider& sources)
{
    std::map<std::pair<std::type_index, uint32_t>, ShaderPtr> built;
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
            built[{ type.type, permutation }] = type.create(rhi, output, permutation);
        }
        catch (const Error& error)
        {
            throw Error(std::string(type.name) + " (permutation " + std::to_string(permutation) + "): " + error.what());
        }
    }
    for (std::pair<const std::pair<std::type_index, uint32_t>, ShaderPtr>& entry : built)
    {
        m_shaders[entry.first] = std::move(entry.second);
    }
}

void ShaderLibrary::compile(IRHI& rhi, ShaderCache& cache, const ShaderType& type)
{
    compile(rhi, cache, type, EmbeddedShaderSourceProvider());
}

void ShaderLibrary::compile_all(IRHI& rhi, ShaderCache& cache, const IShaderSourceProvider& sources)
{
    for (const ShaderType& type : registered_shader_types())
    {
        compile(rhi, cache, type, sources);
    }
}

void ShaderLibrary::compile_all(IRHI& rhi, ShaderCache& cache)
{
    compile_all(rhi, cache, EmbeddedShaderSourceProvider());
}

void ShaderLibrary::load_cooked(IRHI& rhi, const IShaderBinaryStore& store)
{
    ShaderMap map;
    if (!read_shader_map(store, shader_map_id(registered_shader_types()), map))
    {
        throw Error("no cooked shader map for this build's shader types", "run the shader cook for this build");
    }
    std::map<std::pair<std::type_index, uint32_t>, ShaderPtr> built;
    for (const ShaderType& type : registered_shader_types())
    {
        for (uint32_t permutation = 0; permutation < SHADER_MAX_PERMUTATIONS; ++permutation)
        {
            if (!type.should_compile(permutation))
            {
                continue;
            }
            const std::string where = std::string(type.name) + " (permutation " + std::to_string(permutation) + ")";
            const ShaderMapEntry* entry = map.find(type.name, permutation);
            if (entry == nullptr)
            {
                throw Error(where + ": not in the cooked shader map");
            }
            ShaderCompilerOutput output;
            if (!read_shader_output(store, entry->hash, output))
            {
                throw Error(where + ": compiled shader is missing or unreadable in the cooked store");
            }
            try
            {
                built[{ type.type, permutation }] = type.create(rhi, output, permutation);
            }
            catch (const Error& error)
            {
                throw Error(where + ": " + error.what());
            }
        }
    }
    for (std::pair<const std::pair<std::type_index, uint32_t>, ShaderPtr>& entry : built)
    {
        m_shaders[entry.first] = std::move(entry.second);
    }
}

void ShaderLibrary::reload(IRHI& rhi, ShaderCache& cache, const ShaderType& type, const IShaderSourceProvider& sources)
{
    compile(rhi, cache, type, sources);
}

void ShaderLibrary::reload_all(IRHI& rhi, ShaderCache& cache, const IShaderSourceProvider& sources)
{
    std::map<std::pair<std::type_index, uint32_t>, ShaderPtr> previous = m_shaders;
    try
    {
        compile_all(rhi, cache, sources);
    }
    catch (const Error&)
    {
        m_shaders = std::move(previous);
        throw;
    }
}

const ShaderPtr& ShaderLibrary::find(std::type_index type, uint32_t permutation) const
{
    const std::map<std::pair<std::type_index, uint32_t>, ShaderPtr>::const_iterator it = m_shaders.find({ type, permutation });
    if (it == m_shaders.end())
    {
        throw Error(std::string("shader '") + type.name() + "' permutation " + std::to_string(permutation) + " is not in the library");
    }
    return it->second;
}

} // namespace oryx
