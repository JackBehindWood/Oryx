#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include "Oryx/Scripting/Support/InitGuard.h"
#include "Oryx/Scripting/Support/ScriptAssert.h"

namespace py = pybind11;

namespace oryx::python
{

void bind_assertions(py::module_& module)
{
    py::module_ assertions = module.def_submodule("assertions", "Checks that log and raise OryxAssertionError instead of trapping the process.");
    assertions.def("check", OX_GUARDED_FUNC(script_check, "oryx.assertions.check"), py::arg("condition"), py::arg("message") = "");
}

} // namespace oryx::python
