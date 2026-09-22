#pragma once

// On by default, matching OX_ENABLE_ASSERTS/OX_ENABLE_PYTHON; a deliberate debug/test build can
// define this to 0 to bypass the guard for both hosts at once (it cannot be host-specific - see
// the comment on is_embedded_host()).
#ifndef OX_PYTHON_ENFORCE_HOST_GUARD
    #define OX_PYTHON_ENFORCE_HOST_GUARD 1
#endif

namespace oryx::python
{

// Set once by PythonRuntime::start() - this process is embedding Oryx (e.g. inside Oasis).
// Never set for a plain `import oryx` from a standalone Python process, which never calls
// PythonRuntime::start() at all. A runtime flag, not a macro: BindOryx.cpp is compiled once and
// whole-archived into both Oasis and OryxPython, so nothing at compile time can tell them apart.
void mark_embedded_host();
[[nodiscard]] bool is_embedded_host();

} // namespace oryx::python
