#pragma once

#include <pybind11/pybind11.h>

namespace oryx::python
{

// Adds the native module to the interpreter's builtin table; must run before the interpreter starts.
void register_oryx_module();

void bind_oryx(pybind11::module_& module);

void bind_errors(pybind11::module_& module);
void bind_log(pybind11::module_& module);
void bind_assertions(pybind11::module_& module);

} // namespace oryx::python
