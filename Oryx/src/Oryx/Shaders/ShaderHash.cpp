#include "oxpch.h"
#include "Oryx/Shaders/ShaderHash.h"

#include "Oryx/Core/Fnv.h"
#include "Oryx/Shaders/ShaderInclude.h"

namespace oryx
{

ShaderHash hash_shader_input(const ShaderCompilerInput& input, const char* compiler_id, uint32_t compiler_version)
{
    Fnv1a hash;
    hash.mix_value(static_cast<uint64_t>(input.source.language));
    hash.mix_string(input.source.text);
    const ShaderIncludeLookup lookup = [&input](const std::string& name) -> const std::string* {
        const std::map<std::string, std::string>::const_iterator it = input.includes.find(name);
        return it != input.includes.end() ? &it->second : find_shader_include(name);
    };
    for (const std::string& name : shader_include_closure(input.source.text, lookup))
    {
        hash.mix_string(name);
        const std::string* text = lookup(name);
        hash.mix_string(text != nullptr ? *text : std::string());
    }
    hash.mix_string(input.entry_point);
    hash.mix_value(static_cast<uint64_t>(input.stage));

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

} // namespace oryx
