#include "oxpch.h"
#include "Support/PyUtil.h"

#include "Oryx/Scripting/Support/ScriptUtil.h"

namespace py = pybind11;

namespace oryx::python
{

std::string type_name_of(const py::handle& value)
{
    return py::str(py::type::of(value).attr("__name__"));
}

ScriptError to_script_error(const py::error_already_set& error, const std::string& context)
{
    std::string detail = error.what();
    return script_error(context, detail.substr(0, detail.find('\n')), detail);
}

void rethrow_if_interpreter_control(const py::error_already_set& error)
{
    for (PyObject* type : { PyExc_KeyboardInterrupt, PyExc_SystemExit, PyExc_GeneratorExit, PyExc_MemoryError, PyExc_RecursionError })
    {
        if (error.matches(type))
        {
            throw error;
        }
    }
}

} // namespace oryx::python
