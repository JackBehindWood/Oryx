#pragma once

#include "Interop/PyRef.h"

namespace oryx::python
{

// Turns the Python exception that is currently set into a ScriptError carrying its traceback; the GIL must be held.
[[noreturn]] void throw_python_error(const std::string& context);

// "<context>: expected <expected>, got <type of object>".
[[noreturn]] void throw_wrong_type(const std::string& context, const char* expected, PyObject* object);

} // namespace oryx::python
