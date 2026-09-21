#include "oxpch.h"
#include "Support/PySchema.h"

#include "PythonContext.h"
#include "PythonLanguage.h"
#include "Oryx/Scripting/Support/ScriptError.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

std::string class_name(const py::handle& cls)
{
    return cls.attr("__name__").cast<std::string>();
}

bool is_class_var(const py::object& annotation)
{
    py::module_ typing = py::module_::import("typing");
    if (py::isinstance<py::str>(annotation))
    {
        return annotation.cast<std::string>().rfind("ClassVar", 0) == 0 || annotation.cast<std::string>().rfind("typing.ClassVar", 0) == 0;
    }
    return annotation.is(typing.attr("ClassVar")) || typing.attr("get_origin")(annotation).is(typing.attr("ClassVar"));
}

ParamType param_type_of_annotation(const std::string& owner, const std::string& field, const py::object& annotation)
{
    py::object builtins = py::module_::import("builtins");
    std::string text = py::isinstance<py::str>(annotation) ? annotation.cast<std::string>() : "";

    if (annotation.is(builtins.attr("bool")) || text == "bool")
    {
        return ParamType::Bool;
    }
    if (annotation.is(builtins.attr("int")) || text == "int")
    {
        return ParamType::Int;
    }
    if (annotation.is(builtins.attr("float")) || text == "float")
    {
        return ParamType::Double;
    }
    if (annotation.is(builtins.attr("str")) || text == "str")
    {
        return ParamType::String;
    }
    throw ScriptError(owner + "." + field + " is annotated with an unsupported type (fields are parameters of type bool, int, float or str; use typing.ClassVar for anything else)");
}

ParamSpec spec_with_default(const std::string& owner, const std::string& field, ParamType type, const py::object& value)
{
    ParamSpec spec;
    spec.name = field;
    spec.type = type;
    spec.has_default = true;

    bool is_bool = py::isinstance<py::bool_>(value);
    bool is_int = py::isinstance<py::int_>(value) && !is_bool;
    if (type == ParamType::Bool && is_bool)
    {
        spec.default_value = value.cast<bool>();
    }
    else if (type == ParamType::Int && is_int)
    {
        spec.default_value = value.cast<int64_t>();
    }
    else if (type == ParamType::Double && (is_int || py::isinstance<py::float_>(value)))
    {
        spec.default_value = value.cast<double>();
    }
    else if (type == ParamType::String && py::isinstance<py::str>(value))
    {
        spec.default_value = value.cast<std::string>();
    }
    else
    {
        throw ScriptError(owner + "." + field + " is annotated as " + to_string(type) + " but its default has another type");
    }
    return spec;
}

void add_or_replace(ParamSchema& schema, ParamSpec spec)
{
    for (ParamSpec& existing : schema)
    {
        if (existing.name == spec.name)
        {
            existing = std::move(spec);
            return;
        }
    }
    schema.push_back(std::move(spec));
}

} // namespace

ScriptOrigin origin_of_class(const py::handle& cls)
{
    ScriptOrigin origin;
    origin.language = kLanguage;
    origin.module = cls.attr("__module__").cast<std::string>();

    py::object modules = py::module_::import("sys").attr("modules");
    if (modules.contains(origin.module.c_str()))
    {
        py::object file = py::getattr(modules[origin.module.c_str()], "__file__", py::none());
        if (py::isinstance<py::str>(file))
        {
            origin.source_file = file.cast<std::string>();
        }
    }
    return origin;
}

ParamSchema schema_of_class(const py::handle& cls)
{
    std::string owner = class_name(cls);
    py::list mro = py::list(cls.attr("__mro__"));
    mro.attr("reverse")();

    ParamSchema schema;
    for (const py::handle& base : mro)
    {
        if (PythonContext::current().is_base_class(base.ptr()))
        {
            continue;
        }

        py::object annotations = py::module_::import("inspect").attr("get_annotations")(base);
        if (py::len(annotations) == 0)
        {
            continue;
        }

        for (const auto& item : annotations.cast<py::dict>())
        {
            std::string field = py::str(item.first);
            py::object annotation = py::reinterpret_borrow<py::object>(item.second);
            if (is_class_var(annotation))
            {
                continue;
            }
            if (field == "name" || field == "num_players")
            {
                throw ScriptError(owner + "." + field + " is reserved and cannot be a parameter; assign it without an annotation");
            }

            ParamType type = param_type_of_annotation(owner, field, annotation);
            if (py::hasattr(cls, field.c_str()))
            {
                add_or_replace(schema, spec_with_default(owner, field, type, cls.attr(field.c_str())));
            }
            else
            {
                add_or_replace(schema, required_param(field, type));
            }
        }
    }
    return schema;
}

} // namespace oryx::python
