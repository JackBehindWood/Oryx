#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include <pybind11/stl.h>

#include "PyScripted.h"
#include "Interop/PyRef.h"
#include "PythonContext.h"
#include "PythonLanguage.h"
#include "Support/PyHandles.h"
#include "Support/PyParams.h"
#include "Support/PyResolve.h"
#include "Support/PySchema.h"
#include "Support/PyTypeHints.h"
#include "Support/PyUtil.h"
#include "Oryx/Scripting/Registry/ScriptRegistry.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

enum class Kind
{
    Game,
    Strategy
};

const char* kind_name(Kind kind)
{
    return kind == Kind::Game ? "game" : "strategy";
}

const char* required_method(Kind kind)
{
    return kind == Kind::Game ? "new_initial_state" : "decide";
}

// A base class's placeholder does not count; an instance may also carry the method as an attribute.
bool defines(const py::handle& source, const char* method)
{
    py::object owner = PyType_Check(source.ptr()) ? py::reinterpret_borrow<py::object>(source) : py::reinterpret_borrow<py::object>(py::type::of(source));
    py::object found = py::module_::import("inspect").attr("getattr_static")(owner, method, py::none());
    if (!found.is_none())
    {
        return !PythonContext::current().is_placeholder(found.ptr());
    }
    return !PyType_Check(source.ptr()) && py::hasattr(source, method);
}

std::string description_of(const py::handle& callable)
{
    py::object doc = py::getattr(callable, "__doc__", py::none());
    if (!py::isinstance<py::str>(doc))
    {
        return "";
    }
    std::string cleaned = py::module_::import("inspect").attr("cleandoc")(doc).cast<std::string>();
    return cleaned.substr(0, cleaned.find('\n'));
}

ScriptOrigin origin_of_caller()
{
    py::dict globals = py::globals();
    ScriptOrigin origin;
    origin.language = kLanguage;
    origin.module = globals.contains("__name__") ? globals["__name__"].cast<std::string>() : "__main__";
    if (globals.contains("__file__"))
    {
        origin.source_file = globals["__file__"].cast<std::string>();
    }
    return origin;
}

// A class gets its parameters as attributes before __init__ runs, so __init__ can read them; a factory function receives them as keywords.
py::object construct(const py::object& source, bool is_class, Kind kind, const std::string& id, const Params& params)
{
    py::object instance;
    try
    {
        if (is_class)
        {
            instance = source.attr("__new__")(source);
            for (const auto& [key, value] : params)
            {
                instance.attr(key.c_str()) = to_python(value);
            }
            instance.attr("__init__")();
        }
        else
        {
            py::dict kwargs;
            for (const auto& [key, value] : params)
            {
                kwargs[key.c_str()] = to_python(value);
            }
            instance = source(**kwargs);
        }
    }
    catch (const py::error_already_set& error)
    {
        rethrow_if_interpreter_control(error);
        throw to_script_error(error, "could not create the " + std::string(kind_name(kind)) + " '" + id + "'");
    }

    if (!defines(instance, required_method(kind)))
    {
        throw ScriptError("the " + std::string(kind_name(kind)) + " '" + id + "' must define " + required_method(kind) + "()");
    }
    return instance;
}

void register_from(Kind kind, const std::string& id, const py::object& source, bool is_class, ParamSchema schema, std::string description, ScriptOrigin origin, bool overwrite)
{
    if (!is_class && PyCallable_Check(source.ptr()) == 0)
    {
        throw ScriptError(std::string("the factory for the ") + kind_name(kind) + " '" + id + "' must be callable, got '" + type_name_of(source) + "'");
    }
    if (is_class && !defines(source, required_method(kind)))
    {
        throw ScriptError(source.attr("__name__").cast<std::string>() + " must define " + required_method(kind) + "() to be registered as the " + kind_name(kind) + " '" + id + "'");
    }

    SharedPtr<PyScriptObject> holder = create_shared<PyScriptObject>(PyRef::borrow(source.ptr()));
    SharedPtr<const ScriptOrigin> shared_origin = create_shared<const ScriptOrigin>(origin);
    EntryInfo info{ schema, std::move(description) };

    if (kind == Kind::Game)
    {
        register_scripted_game(id, origin, [=](const Params& params) -> UniquePtr<IGame>
        {
            PyGil gil;
            py::object source_object = py::reinterpret_borrow<py::object>(holder->get());
            return create_unique<PyScriptedGame>(PyRef::borrow(construct(source_object, is_class, kind, id, params).ptr()), shared_origin, schema);
        }, std::move(info), overwrite);
    }
    else
    {
        register_scripted_strategy(id, origin, [=](const Params& params) -> UniquePtr<IStrategy>
        {
            PyGil gil;
            py::object source_object = py::reinterpret_borrow<py::object>(holder->get());
            return create_unique<PyScriptedStrategy>(PyRef::borrow(construct(source_object, is_class, kind, id, params).ptr()), shared_origin);
        }, std::move(info), overwrite);
    }
}

// A subclass without an id is an intermediate base and is not registered.
void register_class(Kind kind, const py::object& cls, const py::object& id, bool overwrite, const py::kwargs& kwargs)
{
    std::string owner = cls.attr("__name__").cast<std::string>();
    for (const auto& item : kwargs)
    {
        throw ScriptError("class " + owner + ": unknown class keyword '" + std::string(py::str(item.first)) + "' (expected id and overwrite)");
    }

    if (id.is_none())
    {
        return;
    }
    std::string name = id.cast<std::string>();
    if (name.empty())
    {
        throw ScriptError("class " + owner + ": id cannot be empty");
    }
    register_from(kind, name, cls, true, schema_of_class(cls), description_of(cls), origin_of_class(cls), overwrite);
    cls.attr(kRegisteredIdAttribute) = name;
}

py::object init_subclass_for(Kind kind)
{
    py::cpp_function hook(
        [kind](const py::object& cls, const hints::typing::Optional<py::str>& id, bool overwrite, const py::kwargs& kwargs) { register_class(kind, cls, id, overwrite, kwargs); },
        py::name("__init_subclass__"), py::arg("cls"), py::kw_only(), py::arg("id") = py::none(), py::arg("overwrite") = false);
    return py::module_::import("builtins").attr("classmethod")(hook);
}

[[noreturn]] void throw_not_implemented(const py::object& self, const char* method)
{
    std::string owner = py::type::of(self).attr("__name__").cast<std::string>();
    PyErr_SetString(PyExc_NotImplementedError, (owner + " must define " + method + "()").c_str());
    throw py::error_already_set();
}

// Placeholders give the bases typed methods for IDEs and stubs; defines() and the method tables ignore them (PythonContext::is_placeholder).
template<typename Function, typename... Extra>
void add_placeholder(py::object& base, const char* name, Function&& function, Extra&&... extra)
{
    base.attr(name) = py::cpp_function(std::forward<Function>(function), py::name(name), py::is_method(base), std::forward<Extra>(extra)...);
}

ParamSchema schema_from_dict(const std::string& id, const py::object& params)
{
    ParamSchema schema;
    if (params.is_none())
    {
        return schema;
    }

    py::object builtins = py::module_::import("builtins");
    for (const auto& item : params.cast<py::dict>())
    {
        std::string key = py::str(item.first);
        py::object value = py::reinterpret_borrow<py::object>(item.second);

        if (value.is(builtins.attr("bool")))
        {
            schema.push_back(required_param(key, ParamType::Bool));
        }
        else if (value.is(builtins.attr("int")))
        {
            schema.push_back(required_param(key, ParamType::Int));
        }
        else if (value.is(builtins.attr("float")))
        {
            schema.push_back(required_param(key, ParamType::Double));
        }
        else if (value.is(builtins.attr("str")))
        {
            schema.push_back(required_param(key, ParamType::String));
        }
        else
        {
            ParamValue converted = to_param_value(id, key, value);
            ParamSpec spec;
            spec.name = key;
            spec.type = param_type_of(converted);
            spec.default_value = converted;
            spec.has_default = true;
            schema.push_back(spec);
        }
    }
    return schema;
}

void register_game(const std::string& id, const hints::Factory& factory, const hints::OptionalAnyDict& params, const std::string& description, bool overwrite)
{
    register_from(Kind::Game, id, factory, false, schema_from_dict(id, params), description, origin_of_caller(), overwrite);
}

void register_strategy(const std::string& id, const hints::Factory& factory, const hints::OptionalAnyDict& params, const std::string& description, bool overwrite)
{
    register_from(Kind::Strategy, id, factory, false, schema_from_dict(id, params), description, origin_of_caller(), overwrite);
}

py::object make_base(py::module_& module, const char* name, const char* doc)
{
    py::dict members;
    members["__module__"] = std::string(kModuleName) + ".game";
    members["__doc__"] = doc;
    py::object cls = py::module_::import("builtins").attr("type")(name, py::tuple(), members);
    module.attr(name) = cls;
    return cls;
}

} // namespace

void bind_scripted(py::module_& module)
{
    py::module_ game_module = module.attr("game");
    py::module_ registry = module.attr("registry");

    py::class_<PyActionFeatures, SharedPtr<PyActionFeatures>>(game_module, "ActionFeatures", "Decodes an action into coordinates, for games that provide it; only valid until decide() returns.")
        .def("decode", &PyActionFeatures::decode, py::arg("action"));

    py::class_<PyContext, SharedPtr<PyContext>>(game_module, "Context", "What decide() receives; only valid until decide() returns.")
        .def_property_readonly("state", &PyContext::state)
        .def_property_readonly("action_features", &PyContext::action_features);

    py::object game = make_base(game_module, "Game", "Base class of games defined in Python: `class Nim(oryx.Game, id=\"nim\")` registers on import.");
    game.attr("__init_subclass__") = init_subclass_for(Kind::Game);
    add_placeholder(game, "new_initial_state", [](const py::object& self) -> hints::State { throw_not_implemented(self, "new_initial_state"); },
        "Returns the state a match starts from.");

    py::object strategy = make_base(game_module, "Strategy", "Base class of strategies defined in Python: `class Greedy(oryx.Strategy, id=\"greedy\")` registers on import.");
    strategy.attr("__init_subclass__") = init_subclass_for(Kind::Strategy);
    add_placeholder(strategy, "decide", [](const py::object& self, const hints::Context&) -> ActionId { throw_not_implemented(self, "decide"); },
        py::arg("context"), "Returns the action to play; `context.state` is only valid during the call.");

    py::object state = make_base(game_module, "State", "Optional base class of states: typed method stubs and a default action_to_string().");
    add_placeholder(state, "legal_actions", [](const py::object& self) -> std::vector<ActionId> { throw_not_implemented(self, "legal_actions"); }, "The actions the current player may take.");
    add_placeholder(state, "apply", [](const py::object& self, ActionId) { throw_not_implemented(self, "apply"); }, py::arg("action"), "Plays an action.");
    add_placeholder(state, "undo", [](const py::object& self, ActionId) { throw_not_implemented(self, "undo"); }, py::arg("action"), "Takes back the action that was played last.");
    add_placeholder(state, "current_player", [](const py::object& self) -> int32_t { throw_not_implemented(self, "current_player"); }, "The player to move.");
    add_placeholder(state, "is_terminal", [](const py::object& self) -> bool { throw_not_implemented(self, "is_terminal"); }, "Whether the game is over.");
    add_placeholder(state, "outcome", [](const py::object& self) -> std::vector<double> { throw_not_implemented(self, "outcome"); }, "One reward per player.");
    state.attr("action_to_string") = py::cpp_function([](const py::object&, ActionId action) { return to_string(action); }, py::name("action_to_string"), py::is_method(state), py::arg("action"));


    registry.def("register_game", &register_game, py::arg("id"), py::arg("factory"), py::arg("params") = py::none(), py::arg("description") = "", py::arg("overwrite") = false,
               "Registers a factory function returning a game; `params` maps names to defaults (or to bool/int/float/str for required ones).");
    registry.def("register_strategy", &register_strategy, py::arg("id"), py::arg("factory"), py::arg("params") = py::none(), py::arg("description") = "", py::arg("overwrite") = false,
               "Registers a factory function returning a strategy.");
}

} // namespace oryx::python
