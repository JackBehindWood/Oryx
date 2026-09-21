#pragma once

#include "Interop/PyRef.h"

namespace oryx::python
{

// Holds the GIL for its lifetime; throws ScriptError instead of touching Python while the interpreter is not running.
class PyGil
{
public:
    PyGil();
    ~PyGil();

    PyGil(const PyGil&) = delete;
    PyGil& operator=(const PyGil&) = delete;

private:
    PyGILState_STATE m_state;
};

} // namespace oryx::python
