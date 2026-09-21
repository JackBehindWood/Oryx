#include "oxpch.h"
#include "BindOryx.h"

#include "PyScripted.h"
#include "Oryx/Scripting/ScriptRegistry.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

constexpr const char* kLanguage = "python";
constexpr const char* kMarkerModule = "oryx";

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

void require_params(const std::string& id, const ParamSchema& schema, const Params& params)
{
    for (const ParamSpec& spec : schema)
    {
        if (!has_param(params, spec.name))
        {
            throw ParamError(id, spec.name, "is required");
        }
    }
}

// A class gets its parameters as attributes before __init__ runs, so __init__ can read them; a factory function receives them as keywords.
py::object construct(const py::object& source, bool is_class, Kind kind, const std::string& id, const ParamSchema& schema, const Params& params)
{
    require_params(id, schema, params);

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
        throw to_script_error(error, "could not create the " + std::string(kind_name(kind)) + " '" + id + "'");
    }

    if (!py::hasattr(instance, required_method(kind)))
    {
        throw ScriptError("the " + std::string(kind_name(kind)) + " '" + id + "' must define " + required_method(kind) + "()");
    }
    return instance;
}

void register_from(Kind kind, const std::string& id, const py::object& source, bool is_class, ParamSchema schema, std::string description, ScriptOrigin origin, bool overwrite)
{
    if (is_class && !py::hasattr(source, required_method(kind)))
    {
        throw ScriptError(source.attr("__name__").cast<std::string>() + " must define " + required_method(kind) + "() to be registered as the " + kind_name(kind) + " '" + id + "'");
    }

    SharedPtr<ScriptObject> holder = create_shared<ScriptObject>(source);
    SharedPtr<const ScriptOrigin> shared_origin = create_shared<const ScriptOrigin>(origin);
    EntryInfo info{ schema, std::move(description) };

    if (kind == Kind::Game)
    {
        register_scripted_game(id, origin, [=](const Params& params) -> UniquePtr<IGame>
        {
            py::gil_scoped_acquire gil;
            return create_unique<PyScriptedGame>(construct(holder->get(), is_class, kind, id, schema, params), shared_origin, schema);
        }, std::move(info), overwrite);
    }
    else
    {
        register_scripted_strategy(id, origin, [=](const Params& params) -> UniquePtr<IStrategy>
        {
            py::gil_scoped_acquire gil;
            return create_unique<PyScriptedStrategy>(construct(holder->get(), is_class, kind, id, schema, params), shared_origin);
        }, std::move(info), overwrite);
    }
}

// A subclass without an id is an intermediate base and is not registered.
void register_class(Kind kind, const py::object& cls, const py::kwargs& kwargs)
{
    std::string owner = cls.attr("__name__").cast<std::string>();
    std::string id;
    bool overwrite = false;

    for (const auto& item : kwargs)
    {
        std::string key = py::str(item.first);
        if (key == "id")
        {
            id = py::cast<std::string>(item.second);
        }
        else if (key == "overwrite")
        {
            overwrite = py::cast<bool>(item.second);
        }
        else
        {
            throw ScriptError("class " + owner + ": unknown class keyword '" + key + "' (expected id and overwrite)");
        }
    }

    if (id.empty())
    {
        return;
    }
    register_from(kind, id, cls, true, schema_of_class(cls), description_of(cls), origin_of_class(cls), overwrite);
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
            schema.push_back(param_without_default(key, ParamType::Bool));
        }
        else if (value.is(builtins.attr("int")))
        {
            schema.push_back(param_without_default(key, ParamType::Int));
        }
        else if (value.is(builtins.attr("float")))
        {
            schema.push_back(param_without_default(key, ParamType::Double));
        }
        else if (value.is(builtins.attr("str")))
        {
            schema.push_back(param_without_default(key, ParamType::String));
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

void register_game(const std::string& id, const py::object& factory, const py::object& params, const std::string& description, bool overwrite)
{
    register_from(Kind::Game, id, factory, false, schema_from_dict(id, params), description, origin_of_caller(), overwrite);
}

void register_strategy(const std::string& id, const py::object& factory, const py::object& params, const std::string& description, bool overwrite)
{
    register_from(Kind::Strategy, id, factory, false, schema_from_dict(id, params), description, origin_of_caller(), overwrite);
}

py::object as_classmethod(const py::cpp_function& function)
{
    return py::module_::import("builtins").attr("classmethod")(function);
}

py::object make_base(py::module_& module, const char* name, const char* doc)
{
    py::dict members;
    members["__module__"] = kMarkerModule;
    members["__doc__"] = doc;
    py::object cls = py::module_::import("builtins").attr("type")(name, py::tuple(), members);
    module.attr(name) = cls;
    return cls;
}

} // namespace

void bind_scripted(py::module_& module)
{
    py::class_<IActionFeatures>(module, "ActionFeatures", "Decodes an action into coordinates, for games that provide it.")
        .def("decode", [](const IActionFeatures& features, ActionId action)
            {
                SmallVector<int32_t, 2> decoded = features.decode(action);
                return std::vector<int32_t>(decoded.begin(), decoded.end());
            }, py::arg("action"));

    py::class_<PyContext, SharedPtr<PyContext>>(module, "Context", "What decide() receives; only valid until decide() returns.")
        .def_property_readonly("state", &PyContext::state)
        .def_property_readonly("action_features", &PyContext::action_features, py::return_value_policy::reference);

    py::object game = make_base(module, "Game", "Base class of games defined in Python: `class Nim(oryx.Game, id=\"nim\")` registers on import.");
    game.attr("__init_subclass__") = as_classmethod(py::cpp_function([](const py::object& cls, const py::kwargs& kwargs) { register_class(Kind::Game, cls, kwargs); }));

    py::object strategy = make_base(module, "Strategy", "Base class of strategies defined in Python: `class Greedy(oryx.Strategy, id=\"greedy\")` registers on import.");
    strategy.attr("__init_subclass__") = as_classmethod(py::cpp_function([](const py::object& cls, const py::kwargs& kwargs) { register_class(Kind::Strategy, cls, kwargs); }));

    py::object state = make_base(module, "State", "Optional base class of states; supplies a default action_to_string().");
    state.attr("action_to_string") = py::cpp_function([](const py::object&, ActionId action) { return to_string(action); }, py::name("action_to_string"), py::is_method(state), py::arg("action"));

    module.def("register_game", &register_game, py::arg("id"), py::arg("factory"), py::arg("params") = py::none(), py::arg("description") = "", py::arg("overwrite") = false,
               "Registers a factory function returning a game; `params` maps names to defaults (or to bool/int/float/str for required ones).");
    module.def("register_strategy", &register_strategy, py::arg("id"), py::arg("factory"), py::arg("params") = py::none(), py::arg("description") = "", py::arg("overwrite") = false,
               "Registers a factory function returning a strategy.");
}

} // namespace oryx::python
