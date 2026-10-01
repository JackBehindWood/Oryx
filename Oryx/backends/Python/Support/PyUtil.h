#pragma once

#include <pybind11/pybind11.h>

#include "Oryx/Scripting/Support/ScriptError.h"

namespace oryx::python
{

[[nodiscard]] std::string type_name_of(const pybind11::handle& value);

[[nodiscard]] std::string escape_html(const std::string& text);

// Imports an optional dependency, or throws an Error naming the method that needs it and how to install it.
[[nodiscard]] pybind11::module_ import_optional(const char* name, const char* method);

// The first line of the Python error as the message, the full traceback as detail.
[[nodiscard]] ScriptError to_script_error(const pybind11::error_already_set& error, const std::string& context);

// KeyboardInterrupt, SystemExit, GeneratorExit, MemoryError and RecursionError must reach a Python caller untouched, not as ScriptError.
void rethrow_if_interpreter_control(const pybind11::error_already_set& error);

} // namespace oryx::python
