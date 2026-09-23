#pragma once

#include "Interop/PyRef.h"

namespace oryx::python
{

// Per-interpreter state: the marker classes scripts derive from. Created on first use, dropped before the interpreter is finalised.
class PythonContext
{
    struct Token
    {
    };

public:
    // Construct through current(); Token keeps the constructor out of reach elsewhere.
    explicit PythonContext(Token);

    // The GIL must be held.
    static PythonContext& current();
    static void reset();
    // Resets for good: current() raises from then on, so nothing re-creates the context while the interpreter finalises.
    static void shut_down();

    [[nodiscard]] bool is_game(PyObject* value) const;
    [[nodiscard]] bool is_strategy(PyObject* value) const;
    [[nodiscard]] bool is_game_class(PyObject* value) const;
    [[nodiscard]] bool is_strategy_class(PyObject* value) const;
    // A base class's own NotImplementedError version of a required method, which does not count as defining it.
    [[nodiscard]] bool is_placeholder(PyObject* attribute) const;
    // True for `object` and for the marker classes themselves, which contribute no parameters.
    [[nodiscard]] bool is_base_class(PyObject* cls) const;

private:
    template<typename Methods>
    void collect_placeholders(PyObject* base);

    PyRef m_object;
    PyRef m_game;
    PyRef m_strategy;
    PyRef m_state;
    std::vector<PyRef> m_placeholders;
};

} // namespace oryx::python
