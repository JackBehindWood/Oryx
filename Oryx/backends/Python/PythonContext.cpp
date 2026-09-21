#include "oxpch.h"
#include "PythonContext.h"

#include "PythonLanguage.h"

#include "Interop/PyMethods.h"

namespace oryx::python
{

namespace
{

UniquePtr<PythonContext>& instance()
{
    static UniquePtr<PythonContext> context;
    return context;
}

} // namespace

PythonContext::PythonContext()
{
    PyRef module = PyRef::steal(PyImport_ImportModule(kModuleName));
    if (!module)
    {
        throw_python_error("could not import the native oryx module");
    }

    PyRef builtins = PyRef::steal(PyImport_ImportModule("builtins"));
    m_object = PyRef::steal(PyObject_GetAttrString(builtins.get(), "object"));
    m_game = PyRef::steal(PyObject_GetAttrString(module.get(), "Game"));
    m_strategy = PyRef::steal(PyObject_GetAttrString(module.get(), "Strategy"));
    m_state = PyRef::steal(PyObject_GetAttrString(module.get(), "State"));
    if (!m_object || !m_game || !m_strategy || !m_state)
    {
        throw_python_error("the native oryx module has no Game, Strategy and State");
    }
}

PythonContext& PythonContext::current()
{
    UniquePtr<PythonContext>& context = instance();
    if (!context)
    {
        context.reset(new PythonContext());
    }
    return *context;
}

void PythonContext::reset()
{
    reset_class_method_cache();
    instance().reset();
}

bool PythonContext::is_game(PyObject* value) const
{
    return PyObject_IsInstance(value, m_game.get()) == 1;
}

bool PythonContext::is_strategy(PyObject* value) const
{
    return PyObject_IsInstance(value, m_strategy.get()) == 1;
}

bool PythonContext::is_base_class(PyObject* cls) const
{
    return cls == m_object.get() || cls == m_game.get() || cls == m_strategy.get() || cls == m_state.get();
}

} // namespace oryx::python
