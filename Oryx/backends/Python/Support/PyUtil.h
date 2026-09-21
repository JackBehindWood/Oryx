#pragma once

#include <pybind11/pybind11.h>

#include "Oryx/Scripting/Support/ScriptError.h"

namespace oryx::python
{

[[nodiscard]] std::string type_name_of(const pybind11::handle& value);

// The first line of the Python error as the message, the full traceback as detail.
[[nodiscard]] ScriptError to_script_error(const pybind11::error_already_set& error, const std::string& context);

} // namespace oryx::python
