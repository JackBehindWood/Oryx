#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Error.h"

namespace oryx
{

using ParamValue = std::variant<bool, int64_t, double, std::string>;
using Params = std::map<std::string, ParamValue>;

// Enumerator order matches ParamValue's alternatives.
enum class ParamType
{
    Bool,
    Int,
    Double,
    String
};

struct ParamSpec
{
    std::string name;
    ParamType type = ParamType::Int;
    ParamValue default_value;
    bool has_default = false;
    std::string description;
};

using ParamSchema = std::vector<ParamSpec>;

class ParamError : public Error
{
public:
    ParamError(const std::string& entry, const std::string& key, const std::string& reason)
        : Error("'" + entry + "': parameter '" + key + "' " + reason)
        , m_key(key)
    {
    }

    [[nodiscard]] const char* category() const noexcept override { return "param"; }

    [[nodiscard]] const std::string& key() const { return m_key; }

private:
    std::string m_key;
};

[[nodiscard]] ParamType param_type_of(const ParamValue& value);
[[nodiscard]] std::string to_string(ParamType type);

[[nodiscard]] ParamSpec bool_param(std::string name, bool default_value, std::string description = "");
[[nodiscard]] ParamSpec int_param(std::string name, int64_t default_value, std::string description = "");
[[nodiscard]] ParamSpec double_param(std::string name, double default_value, std::string description = "");
[[nodiscard]] ParamSpec string_param(std::string name, std::string default_value, std::string description = "");
[[nodiscard]] ParamSpec param_without_default(std::string name, ParamType type, std::string description = "");

[[nodiscard]] Params resolve_params(const std::string& entry, const ParamSchema& schema, const Params& provided);

[[nodiscard]] inline bool has_param(const Params& params, const std::string& key)
{
    return params.find(key) != params.end();
}

template<typename T>
[[nodiscard]] const T& get_param(const Params& params, const std::string& key)
{
    return std::get<T>(params.at(key));
}

} // namespace oryx
