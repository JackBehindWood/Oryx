#include "oxpch.h"
#include "BindOryx.h"

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

PyObject* add_exception(py::module_& module, const char* name, PyObject* base)
{
    std::string qualified = std::string("oryx.") + name;
    PyObject* type = PyErr_NewException(qualified.c_str(), base, nullptr);
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
    g_error_type = add_exception(module, "OryxError", PyExc_Exception);
    g_param_error_type = add_exception(module, "ParamError", g_error_type);
    g_script_error_type = add_exception(module, "ScriptError", g_error_type);
    g_assertion_error_type = add_exception(module, "OryxAssertionError", g_error_type);

    py::register_exception_translator(&translate_error);
}

} // namespace oryx::python
