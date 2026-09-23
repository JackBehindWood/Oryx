#pragma once

#include <pybind11/pybind11.h>

namespace oryx::python
{

// Adds the native module to the embedded interpreter's builtin table; must run before the interpreter starts.
void register_oryx_module();

// Everything both hosts share; the research host adds init() on top (OryxPython/src/Module.cpp).
void bind_oryx(pybind11::module_& module);

[[nodiscard]] std::string version_string();

void bind_errors(pybind11::module_& module);
void bind_debug(pybind11::module_& module);
void bind_game(pybind11::module_& module);
void bind_registry(pybind11::module_& module);
void bind_results(pybind11::module_& module);
void bind_simulation(pybind11::module_& module);
void bind_random(pybind11::module_& module);
void bind_math(pybind11::module_& module);
void bind_benchmark(pybind11::module_& module);
void bind_scripted(pybind11::module_& module);

} // namespace oryx::python
