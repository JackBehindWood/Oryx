#include "oxpch.h"
#include "Oryx/Shaders/ShaderHash.h"

#include "Oryx/Shaders/ShaderInclude.h"

namespace oryx
{

namespace
{

constexpr uint64_t FNV_OFFSET = 14695981039346656037ull;
constexpr uint64_t FNV_PRIME = 1099511628211ull;

void mix(uint64_t& hash, const void* data, size_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i)
    {
        hash = (hash ^ bytes[i]) * FNV_PRIME;
    }
}

void mix_value(uint64_t& hash, uint64_t value)
{
    for (uint32_t i = 0; i < 8; ++i)
    {
        const uint8_t byte = static_cast<uint8_t>(value >> (i * 8));
        mix(hash, &byte, 1);
    }
}

// The length prefix keeps adjacent fields from aliasing.
void mix_string(uint64_t& hash, const std::string& text)
{
    mix_value(hash, text.size());
    mix(hash, text.data(), text.size());
}

} // namespace

ShaderHash hash_shader_input(const ShaderCompilerInput& input, const char* compiler_id, uint32_t compiler_version)
{
    uint64_t hash = FNV_OFFSET;
    mix_value(hash, static_cast<uint64_t>(input.source.language));
    mix_string(hash, input.source.text);
    for (const std::string& name : shader_include_closure(input.source.text))
    {
        mix_string(hash, name);
        const std::string* text = find_shader_include(name);
        mix_string(hash, text != nullptr ? *text : std::string());
    }
    mix_string(hash, input.entry_point);
    mix_value(hash, static_cast<uint64_t>(input.stage));

    std::vector<ShaderDefine> defines = input.defines;
    std::sort(defines.begin(), defines.end(), [](const ShaderDefine& a, const ShaderDefine& b) {
        return a.name != b.name ? a.name < b.name : a.value < b.value;
    });
    mix_value(hash, defines.size());
    for (const ShaderDefine& define : defines)
    {
        mix_string(hash, define.name);
        mix_string(hash, define.value);
    }

    mix_string(hash, compiler_id);
    mix_value(hash, compiler_version);
    return hash;
}

} // namespace oryx
