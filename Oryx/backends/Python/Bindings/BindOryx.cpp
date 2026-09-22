#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include "Oryx/Core/Application.h"
#include "Oryx/Core/Assert.h"
#include "Oryx/Core/Error.h"
#include "PythonHost.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

// Order matters: the exception types come first and scripted classes after the handles they refer to.
constexpr std::array<void (*)(py::module_&), 10> kBinders = {
    &bind_errors, &bind_debug, &bind_game, &bind_registry, &bind_results, &bind_simulation, &bind_random, &bind_math, &bind_benchmark, &bind_scripted,
};

// Each name is defined in one submodule and also reachable at the top level, so oryx.Match is oryx.simulation.Match.
void reexport(py::module_& module, const char* submodule, std::initializer_list<const char*> names)
{
    py::object source = module.attr(submodule);
    for (const char* name : names)
    {
        module.attr(name) = source.attr(name);
    }
}

} // namespace

void bind_oryx(py::module_& module)
{
    module.doc() = "Oryx's scripting API: games, strategies, matches, simulation and debugging.";
    for (void (*binder)(py::module_&) : kBinders)
    {
        binder(module);
    }

    reexport(module, "errors", { "OryxError", "ParamError", "ScriptError", "OryxAssertionError" });
    reexport(module, "game", { "GameHandle", "StateHandle", "StrategyHandle", "Context", "ActionFeatures", "Game", "Strategy", "State" });
    reexport(module, "registry", { "make_game", "make_strategy", "list_games", "list_strategies", "describe_game", "describe_strategy", "register_game", "register_strategy" });
    reexport(module, "results", { "BatchResult" });
    reexport(module, "simulation", { "Match", "BatchRunner", "simulate" });
    reexport(module, "random", { "Random" });

    module.def("init", []()
    {
#if OX_PYTHON_ENFORCE_HOST_GUARD
        if (is_embedded_host())
        {
            throw Error("oryx.init() must not be called inside an embedding host (e.g. Oasis); "
                        "it already initialised Oryx before the interpreter started.");
        }
#endif
        init();
        set_assertion_handler(&throw_on_assertion);
    },
    "Initialises Oryx for a standalone Python process (idempotent) and installs the throwing "
    "assertion handler, so a C++ assert reached from Python raises OryxAssertionError instead of "
    "logging and trapping. Raises if called inside an embedding host such as Oasis.");

    module.def("is_embedded_host", &is_embedded_host,
        "True when running inside an embedding host such as Oasis; false for a standalone "
        "research-host process.");
}

} // namespace oryx::python

PYBIND11_MODULE(oryx, module)
{
    oryx::python::bind_oryx(module);
}

namespace oryx::python
{

void register_oryx_module()
{
    if (PyImport_AppendInittab("oryx", &PyInit_oryx) == -1)
    {
        throw std::runtime_error("could not register the oryx module");
    }
}

} // namespace oryx::python
