#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include "Oryx/Core/Params.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

// Owned by the module's dict; reassigned each time the module is initialised in a fresh interpreter.
PyObject* g_error_type = nullptr;
PyObject* g_param_error_type = nullptr;
PyObject* g_script_error_type = nullptr;
PyObject* g_assertion_error_type = nullptr;
PyObject* g_settings_error_type = nullptr;
PyObject* g_illegal_action_error_type = nullptr;
PyObject* g_not_initialised_error_type = nullptr;

PyObject* add_exception(py::module_& module, const char* name, PyObject* base, PyObject* builtin = nullptr)
{
    std::string qualified = std::string("oryx.errors.") + name;
    py::object bases = builtin == nullptr ? py::reinterpret_borrow<py::object>(base) : py::make_tuple(py::handle(base), py::handle(builtin));
    PyObject* type = PyErr_NewException(qualified.c_str(), bases.ptr(), nullptr);
    if (!type)
    {
        throw py::error_already_set();
    }
    module.attr(name) = py::reinterpret_steal<py::object>(type);
    return type;
}

PyObject* exception_type_for(const Error& error)
{
    std::string_view category = error.category();
    if (category == "param")
    {
        return g_param_error_type;
    }
    if (category == "script")
    {
        return g_script_error_type;
    }
    if (category == "assertion")
    {
        return g_assertion_error_type;
    }
    if (category == "settings")
    {
        return g_settings_error_type;
    }
    if (category == "illegal_action")
    {
        return g_illegal_action_error_type;
    }
    if (category == "not_initialised")
    {
        return g_not_initialised_error_type;
    }
    return g_error_type;
}

void translate_error(std::exception_ptr pointer)
{
    if (!pointer)
    {
        return;
    }

    try
    {
        std::rethrow_exception(pointer);
    }
    catch (const Error& error)
    {
        PyObject* type = exception_type_for(error);
        py::object instance = py::handle(type)(error.what());
        instance.attr("detail") = error.detail();
        if (const ParamError* param_error = dynamic_cast<const ParamError*>(&error))
        {
            instance.attr("key") = param_error->key();
        }
        PyErr_SetObject(type, instance.ptr());
    }
}

} // namespace

void bind_errors(py::module_& module)
{
    py::module_ errors = module.def_submodule("errors", "Exceptions Oryx raises.");
    g_error_type = add_exception(errors, "OryxError", PyExc_Exception);
    g_param_error_type = add_exception(errors, "ParamError", g_error_type, PyExc_ValueError);
    g_script_error_type = add_exception(errors, "ScriptError", g_error_type);
    g_assertion_error_type = add_exception(errors, "OryxAssertionError", g_error_type, PyExc_AssertionError);
    g_settings_error_type = add_exception(errors, "SettingsError", g_error_type);
    g_illegal_action_error_type = add_exception(errors, "IllegalActionError", g_script_error_type, PyExc_ValueError);
    g_not_initialised_error_type = add_exception(errors, "NotInitialisedError", g_error_type, PyExc_RuntimeError);

    py::register_exception_translator(&translate_error);
}

} // namespace oryx::python
