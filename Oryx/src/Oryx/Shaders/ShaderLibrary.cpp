#include "oxpch.h"
#include "Oryx/Shaders/ShaderLibrary.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Shaders/ShaderInclude.h"

namespace oryx
{

namespace
{

// Loads the type's source and the closure of its includes; throws Error naming the missing path.
ShaderCompilerInput load_input(const ShaderType& type, const IShaderSourceProvider& sources)
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

    std::vector<std::string> pending = shader_include_closure(input.source.text, [](const std::string&) { return nullptr; });
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
        for (const std::string& nested : shader_include_closure(include->text, [](const std::string&) { return nullptr; }))
        {
            pending.push_back(nested);
        }
        input.includes[name] = std::move(include->text);
    }
    return input;
}

} // namespace

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
            ShaderCompilerInput input = load_input(type, sources);
            input.defines = type.defines_for(permutation);
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
