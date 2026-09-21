#pragma once

#include <pybind11/pybind11.h>

#include "Oryx/Core/Params.h"
#include "Oryx/Scripting/Support/ScriptOrigin.h"

namespace oryx::python
{

// Names a Python class's home: its module and, when it has one, the file that module was loaded from.
[[nodiscard]] ScriptOrigin origin_of_class(const pybind11::handle& cls);

// Typed class fields (bool/int/float/str) become parameters; a class value is the default. Throws ScriptError for anything else.
[[nodiscard]] ParamSchema schema_of_class(const pybind11::handle& cls);

} // namespace oryx::python
