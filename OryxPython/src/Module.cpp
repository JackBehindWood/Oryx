#include "oxpch.h"

#include "Bindings/BindOryx.h"
#include "PythonContext.h"
#include "PythonLanguage.h"
#include "Support/PyScripts.h"
#include "Support/PyTypeHints.h"
#include "Oryx/Core/Application.h"
#include "Oryx/Core/Assert.h"
#include "Oryx/Scripting/Registry/ScriptRegistry.h"

namespace py = pybind11;

namespace
{

void init_research_host(const oryx::python::hints::PathArg& settings)
{
    oryx::init();
    oryx::set_assertion_handler(&oryx::throw_on_assertion);

    std::filesystem::path file = settings.is_none() ? std::filesystem::path() : std::filesystem::path(py::module_::import("os").attr("fspath")(settings).cast<std::string>());
    oryx::python::load_configured_scripts(file);
}

// Runs before Py_FinalizeEx, while the objects these statics hold can still be released.
void tear_down_research_host()
{
    oryx::unregister_scripted(oryx::python::kLanguage);
    oryx::python::PythonContext::shut_down();
}

} // namespace

PYBIND11_MODULE(oryx, module)
{
    oryx::python::bind_oryx(module);

    module.def("init", &init_research_host, py::arg("settings") = py::none(),
        "Initialises Oryx for a standalone Python process and installs the throwing assertion handler, so a C++ assert "
        "reached from Python raises OryxAssertionError. Reads `settings` (else the nearest oryx.yaml above the working "
        "directory, if any) and imports the scripts under its `scripting.roots`; calling it again loads nothing new.");

    py::module_::import("atexit").attr("register")(py::cpp_function(&tear_down_research_host));
}
