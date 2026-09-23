#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include "PythonLanguage.h"

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

    reexport(module, "errors", { "OryxError", "ParamError", "ScriptError", "OryxAssertionError", "SettingsError", "IllegalActionError", "NotInitialisedError" });
    reexport(module, "game", { "GameHandle", "StateHandle", "StrategyHandle", "Context", "ActionFeatures", "Game", "Strategy", "State" });
    reexport(module, "registry", { "make_game", "make_strategy", "list_games", "list_strategies", "describe_game", "describe_strategy", "register_game", "register_strategy" });
    reexport(module, "results", { "BatchResult" });
    reexport(module, "simulation", { "Match", "BatchRunner", "simulate" });
    reexport(module, "random", { "Random" });

    module.attr("__version__") = version_string();
}

std::string version_string()
{
    return std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) + "." + std::to_string(VERSION_PATCH);
}

} // namespace oryx::python

// Registered as "oryx" through the inittab; the PyInit_oryx symbol belongs to the research host's shared library.
PYBIND11_MODULE(oryx_embedded, module)
{
    oryx::python::bind_oryx(module);
}

namespace oryx::python
{

void register_oryx_module()
{
    if (PyImport_AppendInittab(kModuleName, &PyInit_oryx_embedded) == -1)
    {
        throw std::runtime_error("could not register the oryx module");
    }
}

} // namespace oryx::python
