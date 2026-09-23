#include "oxpch.h"
#include "PythonContext.h"

#include "PythonLanguage.h"
#include "Oryx/Core/Error.h"

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

bool g_shut_down = false;

} // namespace

PythonContext::PythonContext(Token)
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
    collect_placeholders<GameMethods>(m_game.get());
    collect_placeholders<StrategyMethods>(m_strategy.get());
    collect_placeholders<StateMethods>(m_state.get());
}

PythonContext& PythonContext::current()
{
    UniquePtr<PythonContext>& context = instance();
    if (g_shut_down) [[unlikely]]
    {
        throw Error("oryx is shutting down: the interpreter is being finalised");
    }
    if (!context)
    {
        context = create_unique<PythonContext>(Token{});
    }
    return *context;
}

void PythonContext::reset()
{
    reset_class_method_cache();
    instance().reset();
}

void PythonContext::shut_down()
{
    reset();
    g_shut_down = true;
}

bool PythonContext::is_game(PyObject* value) const
{
    return PyObject_IsInstance(value, m_game.get()) == 1;
}

bool PythonContext::is_strategy(PyObject* value) const
{
    return PyObject_IsInstance(value, m_strategy.get()) == 1;
}

bool PythonContext::is_game_class(PyObject* value) const
{
    return PyType_Check(value) && PyObject_IsSubclass(value, m_game.get()) == 1;
}

bool PythonContext::is_strategy_class(PyObject* value) const
{
    return PyType_Check(value) && PyObject_IsSubclass(value, m_strategy.get()) == 1;
}

bool PythonContext::is_placeholder(PyObject* attribute) const
{
    return std::any_of(m_placeholders.begin(), m_placeholders.end(), [attribute](const PyRef& placeholder) { return placeholder.get() == attribute; });
}

template<typename Methods>
void PythonContext::collect_placeholders(PyObject* base)
{
    PyObject* own = reinterpret_cast<PyTypeObject*>(base)->tp_dict;
    for (size_t i = 0; i < Methods::required; ++i)
    {
        std::string name(Methods::names[i]);
        if (PyObject* found = PyDict_GetItemString(own, name.c_str()))
        {
            m_placeholders.push_back(PyRef::borrow(found));
        }
    }
}

bool PythonContext::is_base_class(PyObject* cls) const
{
    return cls == m_object.get() || cls == m_game.get() || cls == m_strategy.get() || cls == m_state.get();
}

} // namespace oryx::python
