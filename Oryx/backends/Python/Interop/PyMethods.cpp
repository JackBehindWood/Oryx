#include "oxpch.h"
#include "Interop/PyMethods.h"

#include "PythonContext.h"

namespace oryx::python
{

namespace
{

using CacheKey = std::pair<PyObject*, const void*>;

std::map<CacheKey, SharedPtr<const PyClassMethods>>& cache()
{
    static std::map<CacheKey, SharedPtr<const PyClassMethods>> instance;
    return instance;
}

PyRef plain_function_of(PyObject* raw)
{
    if (PyFunction_Check(raw) != 0)
    {
        return PyRef::borrow(raw);
    }
    if (PyInstanceMethod_Check(raw) != 0)
    {
        return PyRef::borrow(PyInstanceMethod_GET_FUNCTION(raw));
    }
    return PyRef();
}

} // namespace

PyClassMethods::~PyClassMethods()
{
    if (Py_IsInitialized() != 0)
    {
        PyGILState_STATE state = PyGILState_Ensure();
        entries.clear();
        type = PyRef();
        PyGILState_Release(state);
        return;
    }

    for (Entry& entry : entries)
    {
        entry.function.release();
        entry.name.release();
    }
    type.release();
}

void reset_class_method_cache()
{
    cache().clear();
}

SharedPtr<const PyClassMethods> class_methods_for(PyObject* type, const void* set_id, std::span<const std::string_view> names, size_t required, std::string_view owner)
{
    CacheKey key{ type, set_id };
    std::map<CacheKey, SharedPtr<const PyClassMethods>>::const_iterator cached = cache().find(key);
    if (cached != cache().end())
    {
        return cached->second;
    }

    PyRef inspect = PyRef::steal(PyImport_ImportModule("inspect"));
    if (!inspect)
    {
        throw_python_error("could not import inspect");
    }

    SharedPtr<PyClassMethods> methods = create_shared<PyClassMethods>();
    methods->type = PyRef::borrow(type);
    methods->entries.reserve(names.size());
    for (size_t i = 0; i < names.size(); ++i)
    {
        std::string name(names[i]);
        PyClassMethods::Entry entry;
        entry.name = PyRef::steal(PyUnicode_InternFromString(name.c_str()));

        PyRef raw = PyRef::steal(PyObject_CallMethod(inspect.get(), "getattr_static", "OsO", type, name.c_str(), Py_None));
        if (!raw)
        {
            throw_python_error("could not look up " + name + "()");
        }

        bool defined = raw.get() != Py_None && !PythonContext::current().is_placeholder(raw.get());
        if (!defined && i < required)
        {
            throw ScriptError(std::string(owner) + " class '" + reinterpret_cast<PyTypeObject*>(type)->tp_name + "' must define " + name + "()");
        }
        if (defined)
        {
            entry.function = plain_function_of(raw.get());
        }
        methods->entries.push_back(std::move(entry));
    }

    cache().emplace(key, methods);
    return methods;
}

} // namespace oryx::python
