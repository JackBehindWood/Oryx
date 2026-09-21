#include "oxpch.h"
#include "Support/PyParams.h"

#include "Support/PyUtil.h"

namespace py = pybind11;

namespace oryx::python
{

ParamValue to_param_value(const std::string& entry, const std::string& key, const py::handle& value)
{
    if (py::isinstance<py::bool_>(value))
    {
        return value.cast<bool>();
    }
    if (py::isinstance<py::int_>(value))
    {
        return value.cast<int64_t>();
    }
    if (py::isinstance<py::float_>(value))
    {
        return value.cast<double>();
    }
    if (py::isinstance<py::str>(value))
    {
        return value.cast<std::string>();
    }
    if (PyIndex_Check(value.ptr()) != 0)
    {
        return py::reinterpret_steal<py::int_>(PyNumber_Index(value.ptr())).cast<int64_t>();
    }
    if (PyObject_HasAttrString(value.ptr(), "__float__") != 0)
    {
        return value.attr("__float__")().cast<double>();
    }
    throw ParamError(entry, key, "has unsupported type '" + type_name_of(value) + "' (expected bool, int, float or str)");
}

Params to_params(const std::string& entry, const py::kwargs& kwargs)
{
    Params params;
    for (const auto& item : kwargs)
    {
        std::string key = py::str(item.first);
        params.emplace(key, to_param_value(entry, key, item.second));
    }
    return params;
}

py::object to_python(const ParamValue& value)
{
    return std::visit([](const auto& alternative) { return py::cast(alternative); }, value);
}

py::list schema_to_python(const ParamSchema& schema)
{
    py::list result;
    for (const ParamSpec& spec : schema)
    {
        py::dict entry;
        entry["name"] = spec.name;
        entry["type"] = to_string(spec.type);
        entry["description"] = spec.description;
        entry["required"] = spec.required;
        if (spec.has_default)
        {
            entry["default"] = to_python(spec.default_value);
        }
        result.append(entry);
    }
    return result;
}

} // namespace oryx::python
