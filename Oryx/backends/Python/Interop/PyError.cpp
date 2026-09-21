#include "oxpch.h"
#include "Interop/PyError.h"

#include "Support/PyUtil.h"

namespace py = pybind11;

namespace oryx::python
{

void throw_python_error(const std::string& context)
{
    try
    {
        throw py::error_already_set();
    }
    catch (const py::error_already_set& error)
    {
        throw to_script_error(error, context);
    }
}

void throw_wrong_type(const std::string& context, const char* expected, PyObject* object)
{
    throw ScriptError(context + ": expected " + expected + ", got " + Py_TYPE(object)->tp_name);
}

} // namespace oryx::python
