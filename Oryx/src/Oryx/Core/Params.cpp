#include "oxpch.h"
#include "Oryx/Core/Params.h"

namespace oryx
{

namespace
{

const ParamSpec* find_spec(const ParamSchema& schema, const std::string& name)
{
    for (const ParamSpec& spec : schema)
    {
        if (spec.name == name)
        {
            return &spec;
        }
    }
    return nullptr;
}

std::string known_keys(const ParamSchema& schema)
{
    if (schema.empty())
    {
        return "none";
    }

    std::string keys;
    for (const ParamSpec& spec : schema)
    {
        keys += keys.empty() ? spec.name : ", " + spec.name;
    }
    return keys;
}

ParamSpec make_spec(std::string name, ParamType type, ParamValue default_value, bool has_default, std::string description)
{
    return ParamSpec{ std::move(name), type, std::move(default_value), has_default, std::move(description) };
}

} // namespace

ParamType param_type_of(const ParamValue& value)
{
    return static_cast<ParamType>(value.index());
}

std::string to_string(ParamType type)
{
    switch (type)
    {
        case ParamType::Bool: return "bool";
        case ParamType::Int: return "int";
        case ParamType::Double: return "double";
        case ParamType::String: return "string";
    }
    return "unknown";
}

ParamSpec bool_param(std::string name, bool default_value, std::string description)
{
    return make_spec(std::move(name), ParamType::Bool, default_value, true, std::move(description));
}

ParamSpec int_param(std::string name, int64_t default_value, std::string description)
{
    return make_spec(std::move(name), ParamType::Int, default_value, true, std::move(description));
}

ParamSpec double_param(std::string name, double default_value, std::string description)
{
    return make_spec(std::move(name), ParamType::Double, default_value, true, std::move(description));
}

ParamSpec string_param(std::string name, std::string default_value, std::string description)
{
    return make_spec(std::move(name), ParamType::String, std::move(default_value), true, std::move(description));
}

ParamSpec param_without_default(std::string name, ParamType type, std::string description)
{
    return make_spec(std::move(name), type, ParamValue{}, false, std::move(description));
}

Params resolve_params(const std::string& entry, const ParamSchema& schema, const Params& provided)
{
    Params resolved;

    for (const auto& [key, value] : provided)
    {
        const ParamSpec* spec = find_spec(schema, key);
        if (spec == nullptr)
        {
            throw ParamError(entry, key, "is unknown (known parameters: " + known_keys(schema) + ")");
        }

        ParamType given = param_type_of(value);
        if (given == spec->type)
        {
            resolved.emplace(key, value);
        }
        else if (spec->type == ParamType::Double && given == ParamType::Int)
        {
            resolved.emplace(key, static_cast<double>(std::get<int64_t>(value)));
        }
        else
        {
            throw ParamError(entry, key, "expects " + to_string(spec->type) + " but got " + to_string(given));
        }
    }

    for (const ParamSpec& spec : schema)
    {
        if (spec.has_default && !has_param(resolved, spec.name))
        {
            resolved.emplace(spec.name, spec.default_value);
        }
    }

    return resolved;
}

} // namespace oryx
