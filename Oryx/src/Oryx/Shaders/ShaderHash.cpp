#include "oxpch.h"
#include "Oryx/Shaders/ShaderHash.h"

#include "Oryx/Core/Fnv.h"
#include "Oryx/Shaders/ShaderInclude.h"

namespace oryx
{

namespace
{

const std::string* find_include(const ShaderCompilerInput& input, const std::string& name)
{
    const std::map<std::string, std::string>::const_iterator it = input.includes.find(name);
    return it != input.includes.end() ? &it->second : find_shader_include(name);
}

// Every file reachable from the source, sorted and unique; unknown names are listed but not followed.
std::vector<std::string> dependency_closure(const ShaderCompilerInput& input, const IShaderCompiler& compiler)
{
    std::set<std::string> names;
    std::vector<std::string> pending = compiler.dependencies(input.source);
    while (!pending.empty())
    {
        const std::string name = pending.back();
        pending.pop_back();
        if (!names.insert(name).second)
        {
            continue;
        }
        if (const std::string* text = find_include(input, name))
        {
            ShaderSource nested = input.source;
            nested.text = *text;
            for (const std::string& next : compiler.dependencies(nested))
            {
                pending.push_back(next);
            }
        }
    }
    return std::vector<std::string>(names.begin(), names.end());
}

ShaderHash hash_with(const ShaderCompilerInput& input, const IShaderCompiler& compiler, const char* compiler_id, uint32_t compiler_version)
{
    Fnv1a hash;
    hash.mix_value(static_cast<uint64_t>(input.source.language));
    hash.mix_string(input.source.text);
    for (const std::string& name : dependency_closure(input, compiler))
    {
        hash.mix_string(name);
        const std::string* text = find_include(input, name);
        hash.mix_string(text != nullptr ? *text : std::string());
    }
    hash.mix_string(input.entry_point);
    hash.mix_value(static_cast<uint64_t>(input.stage));
    hash.mix_value(static_cast<uint64_t>(input.target));

    std::vector<ShaderDefine> defines = input.defines;
    std::sort(defines.begin(), defines.end(), [](const ShaderDefine& a, const ShaderDefine& b) {
        return a.name != b.name ? a.name < b.name : a.value < b.value;
    });
    hash.mix_value(defines.size());
    for (const ShaderDefine& define : defines)
    {
        hash.mix_string(define.name);
        hash.mix_string(define.value);
    }

    hash.mix_string(compiler_id);
    hash.mix_value(compiler_version);
    return hash.value();
}

} // namespace

ShaderHash hash_shader_input(const ShaderCompilerInput& input, const IShaderCompiler& compiler)
{
    return hash_with(input, compiler, compiler.id(), compiler.version());
}

ShaderHash hash_shader_input(const ShaderCompilerInput& input, const char* compiler_id, uint32_t compiler_version)
{
    return hash_with(input, shader_compiler_for(input.source.language), compiler_id, compiler_version);
}

} // namespace oryx
