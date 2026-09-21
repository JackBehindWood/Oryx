#pragma once

#include "Interop/PyRef.h"

namespace oryx::python
{

// Per-interpreter state: the marker classes scripts derive from. Created on first use, dropped before the interpreter is finalised.
class PythonContext
{
public:
    // The GIL must be held.
    static PythonContext& current();
    static void reset();

    [[nodiscard]] bool is_game(PyObject* value) const;
    [[nodiscard]] bool is_strategy(PyObject* value) const;
    // True for `object` and for the marker classes themselves, which contribute no parameters.
    [[nodiscard]] bool is_base_class(PyObject* cls) const;

private:
    PythonContext();

    PyRef m_object;
    PyRef m_game;
    PyRef m_strategy;
    PyRef m_state;
};

} // namespace oryx::python
