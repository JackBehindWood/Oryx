#pragma once

#include "Interop/PyConvert.h"
#include "Interop/PyError.h"
#include "Interop/PyGil.h"
#include "Interop/PyScriptObject.h"

#include "Oryx/Core/FixedString.h"
#include "Oryx/Scripting/Support/ScriptError.h"

namespace oryx::python
{

// The methods of one scripted interface, named at compile time: the first `Required` are mandatory, the rest optional.
template<FixedString Owner, size_t Required, FixedString... Names>
struct PyMethods
{
    static constexpr size_t count = sizeof...(Names);
    static constexpr size_t required = Required;
    static constexpr std::string_view owner = Owner.view();
    static constexpr std::array<std::string_view, count> names = { Names.view()... };

    template<FixedString Name>
    static constexpr size_t index()
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (names[i] == Name.view())
            {
                return i;
            }
        }
        return count;
    }

    static_assert(Required <= sizeof...(Names));
};

// What one Python class offers for a method set: the unbound function when it can be called as function(self, ...), else only the interned name.
class PyClassMethods
{
public:
    PyClassMethods() = default;
    ~PyClassMethods();

    PyClassMethods(const PyClassMethods&) = delete;
    PyClassMethods& operator=(const PyClassMethods&) = delete;

    struct Entry
    {
        PyRef function;
        PyRef name;
    };

    PyRef type;
    std::vector<Entry> entries;
};

// Builds (or fetches from the cache) the table for `type`; throws ScriptError naming the first missing required method.
[[nodiscard]] SharedPtr<const PyClassMethods> class_methods_for(PyObject* type, const void* set_id, std::span<const std::string_view> names, size_t required, std::string_view owner);

// Drops every cached table; the GIL must be held and the interpreter still running.
void reset_class_method_cache();

template<typename Set>
[[nodiscard]] SharedPtr<const PyClassMethods> class_methods_for(PyObject* object)
{
    return class_methods_for(reinterpret_cast<PyObject*>(Py_TYPE(object)), &Set::names, std::span<const std::string_view>(Set::names), Set::required, Set::owner);
}

template<typename Set, FixedString Name>
[[nodiscard]] std::string method_context()
{
    return std::string(Set::owner) + "." + std::string(Name.view()) + "()";
}

// Calls a method of `object` through its class's table with typed arguments and result; the error context is only built on failure.
template<typename Set, FixedString Name, typename Result = void, typename... Args>
Result call_method(const PyScriptObject& object, const PyClassMethods& methods, const Args&... args)
{
    constexpr size_t index = Set::template index<Name>();
    static_assert(index < Set::count, "the method is not in the set");

    PyGil gil;
    constexpr size_t argc = 1 + sizeof...(Args);
    PyRef holders[argc] = { PyRef::borrow(object.get()), PyConvert<Args>::to_py(args)... };
    PyObject* argv[argc];
    for (size_t i = 0; i < argc; ++i)
    {
        argv[i] = holders[i].get();
    }

    const PyClassMethods::Entry& entry = methods.entries[index];
    PyRef result = entry.function
        ? PyRef::steal(PyObject_Vectorcall(entry.function.get(), argv, argc, nullptr))
        : PyRef::steal(PyObject_VectorcallMethod(entry.name.get(), argv, argc, nullptr));
    if (!result)
    {
        throw_python_error(method_context<Set, Name>());
    }

    if constexpr (!std::is_void_v<Result>)
    {
        Result value{};
        if (!PyConvert<Result>::from_py(result.get(), value))
        {
            throw_wrong_type(method_context<Set, Name>(), PyConvert<Result>::expected, result.get());
        }
        return value;
    }
}

using StateMethods = PyMethods<"state", 6, "legal_actions", "apply", "undo", "current_player", "is_terminal", "outcome", "action_to_string">;
using GameMethods = PyMethods<"game", 1, "new_initial_state">;
using StrategyMethods = PyMethods<"strategy", 1, "decide">;

} // namespace oryx::python
