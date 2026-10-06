#include "oxpch.h"
#include "Oryx/Shaders/ShaderReflection.h"

#include "Oryx/Shaders/Source/ShaderSource.h"

namespace oryx
{

namespace
{

uint32_t scalar_bytes(ShaderScalar scalar)
{
    switch (scalar)
    {
    case ShaderScalar::Half: return 2;
    case ShaderScalar::Bool: return 1;
    case ShaderScalar::Float:
    case ShaderScalar::Int:
    case ShaderScalar::UInt: return 4;
    }
    return 4;
}

const char* scalar_name(ShaderScalar scalar)
{
    switch (scalar)
    {
    case ShaderScalar::Float: return "float";
    case ShaderScalar::Half: return "half";
    case ShaderScalar::Int: return "int";
    case ShaderScalar::UInt: return "uint";
    case ShaderScalar::Bool: return "bool";
    }
    return "float";
}

// A vector (or matrix column) of three occupies the space of four.
uint32_t column_alignment(const ShaderDataType& type)
{
    return scalar_bytes(type.scalar) * (type.rows == 3 ? 4 : type.rows);
}

} // namespace

const char* shader_stage_name(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::Vertex: return "vertex";
    case ShaderStage::Pixel: return "pixel";
    case ShaderStage::Compute: return "compute";
    case ShaderStage::TessControl: return "tess_control";
    case ShaderStage::TessEval: return "tess_eval";
    }
    return "unknown";
}

const char* shader_language_name(ShaderLanguage language)
{
    switch (language)
    {
    case ShaderLanguage::MSL: return "msl";
    case ShaderLanguage::Slang: return "slang";
    }
    return "unknown";
}

uint32_t shader_type_alignment(const ShaderDataType& type)
{
    return column_alignment(type);
}

uint32_t shader_type_size(const ShaderDataType& type)
{
    if (type.columns > 1)
    {
        return type.columns * column_alignment(type);
    }
    return scalar_bytes(type.scalar) * type.rows;
}

std::string shader_type_name(const ShaderDataType& type)
{
    std::string name = scalar_name(type.scalar);
    if (type.columns > 1)
    {
        return name + std::to_string(type.columns) + "x" + std::to_string(type.rows);
    }
    return type.rows > 1 ? name + std::to_string(type.rows) : name;
}

bool parse_shader_type(std::string_view name, ShaderDataType& out)
{
    struct Prefix
    {
        const char* text;
        ShaderScalar scalar;
    };
    static constexpr Prefix prefixes[] = {
        { "float", ShaderScalar::Float }, { "half", ShaderScalar::Half }, { "uint", ShaderScalar::UInt },
        { "int", ShaderScalar::Int }, { "bool", ShaderScalar::Bool }
    };
    for (const Prefix& prefix : prefixes)
    {
        const std::string_view text = prefix.text;
        if (name.substr(0, text.size()) != text)
        {
            continue;
        }
        const std::string_view rest = name.substr(text.size());
        const bool digit_only = rest.size() == 1 && rest[0] >= '2' && rest[0] <= '4';
        const bool matrix = rest.size() == 3 && rest[1] == 'x' && rest[0] >= '2' && rest[0] <= '4' && rest[2] >= '2' && rest[2] <= '4';
        if (rest.empty())
        {
            out = { prefix.scalar, 1, 1 };
            return true;
        }
        if (digit_only)
        {
            out = { prefix.scalar, static_cast<uint8_t>(rest[0] - '0'), 1 };
            return true;
        }
        if (matrix && (prefix.scalar == ShaderScalar::Float || prefix.scalar == ShaderScalar::Half))
        {
            out = { prefix.scalar, static_cast<uint8_t>(rest[2] - '0'), static_cast<uint8_t>(rest[0] - '0') };
            return true;
        }
    }
    return false;
}

const ShaderBinding* find_binding(const ShaderParameterMap& parameters, std::string_view name)
{
    for (const ShaderBinding& binding : parameters)
    {
        if (binding.name == name)
        {
            return &binding;
        }
    }
    return nullptr;
}

} // namespace oryx
