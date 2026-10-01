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


std::string escape_html(const std::string& text)
{
    std::string escaped;
    for (char c : text)
    {
        switch (c)
        {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default: escaped += c; break;
        }
    }
    return escaped;
}

py::module_ import_optional(const char* name, const char* method)
{
    try
    {
        return py::module_::import(name);
    }
    catch (const py::error_already_set& error)
    {
        if (!error.matches(PyExc_ImportError))
        {
            throw;
        }
        throw Error(std::string(method) + " needs " + name + "; install it with `pip install " + name + "`");
    }
}

} // namespace oryx::python
