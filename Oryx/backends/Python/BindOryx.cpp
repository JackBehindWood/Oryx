#include "oxpch.h"
#include "BindOryx.h"

namespace py = pybind11;

namespace oryx::python
{

void bind_oryx(py::module_& module)
{
    module.doc() = "Oryx's native bindings; import the pure-Python 'oryx' package instead.";
    bind_errors(module);
    bind_log(module);
    bind_assertions(module);
    bind_game(module);
    bind_registry(module);
    bind_simulation(module);
    bind_random(module);
    bind_scripted(module);
}

} // namespace oryx::python

PYBIND11_MODULE(_oryx, module)
{
    oryx::python::bind_oryx(module);
}

namespace oryx::python
{

void register_oryx_module()
{
    if (PyImport_AppendInittab("_oryx", &PyInit__oryx) == -1)
    {
        throw std::runtime_error("could not register the _oryx module");
    }
}

} // namespace oryx::python
