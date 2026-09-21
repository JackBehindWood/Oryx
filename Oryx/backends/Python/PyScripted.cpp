#include "oxpch.h"
#include "PyScripted.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

constexpr const char* kLanguage = "python";
constexpr const char* kMarkerModule = "oryx";

template<typename Body>
auto call_script(const std::string& context, Body&& body) -> decltype(body())
{
    try
    {
        return body();
    }
    catch (const py::error_already_set& error)
    {
        throw to_script_error(error, context);
    }
    catch (const py::cast_error& error)
    {
        throw ScriptError(context + ": " + error.what());
    }
}

std::string class_name(const py::handle& cls)
{
    return cls.attr("__name__").cast<std::string>();
}

py::object marker(const char* name)
{
    return py::module_::import("_oryx").attr(name);
}

bool is_marker_or_object(const py::handle& cls)
{
    return cls.is(py::module_::import("builtins").attr("object")) || cls.attr("__module__").cast<std::string>() == kMarkerModule;
}

bool is_class_var(const py::object& annotation)
{
    py::module_ typing = py::module_::import("typing");
    if (py::isinstance<py::str>(annotation))
    {
        return annotation.cast<std::string>().rfind("ClassVar", 0) == 0 || annotation.cast<std::string>().rfind("typing.ClassVar", 0) == 0;
    }
    return annotation.is(typing.attr("ClassVar")) || typing.attr("get_origin")(annotation).is(typing.attr("ClassVar"));
}

ParamType param_type_of_annotation(const std::string& owner, const std::string& field, const py::object& annotation)
{
    py::object builtins = py::module_::import("builtins");
    std::string text = py::isinstance<py::str>(annotation) ? annotation.cast<std::string>() : "";

    if (annotation.is(builtins.attr("bool")) || text == "bool")
    {
        return ParamType::Bool;
    }
    if (annotation.is(builtins.attr("int")) || text == "int")
    {
        return ParamType::Int;
    }
    if (annotation.is(builtins.attr("float")) || text == "float")
    {
        return ParamType::Double;
    }
    if (annotation.is(builtins.attr("str")) || text == "str")
    {
        return ParamType::String;
    }
    throw ScriptError(owner + "." + field + " is annotated with an unsupported type (fields are parameters of type bool, int, float or str; use typing.ClassVar for anything else)");
}

ParamSpec spec_with_default(const std::string& owner, const std::string& field, ParamType type, const py::object& value)
{
    ParamSpec spec;
    spec.name = field;
    spec.type = type;
    spec.has_default = true;

    bool is_bool = py::isinstance<py::bool_>(value);
    bool is_int = py::isinstance<py::int_>(value) && !is_bool;
    if (type == ParamType::Bool && is_bool)
    {
        spec.default_value = value.cast<bool>();
    }
    else if (type == ParamType::Int && is_int)
    {
        spec.default_value = value.cast<int64_t>();
    }
    else if (type == ParamType::Double && (is_int || py::isinstance<py::float_>(value)))
    {
        spec.default_value = value.cast<double>();
    }
    else if (type == ParamType::String && py::isinstance<py::str>(value))
    {
        spec.default_value = value.cast<std::string>();
    }
    else
    {
        throw ScriptError(owner + "." + field + " is annotated as " + to_string(type) + " but its default has another type");
    }
    return spec;
}

void add_or_replace(ParamSchema& schema, ParamSpec spec)
{
    for (ParamSpec& existing : schema)
    {
        if (existing.name == spec.name)
        {
            existing = std::move(spec);
            return;
        }
    }
    schema.push_back(std::move(spec));
}

SharedPtr<const ScriptOrigin> shared_origin_of(const py::object& instance)
{
    return create_shared<const ScriptOrigin>(origin_of_class(py::type::of(instance)));
}

} // namespace

ScriptOrigin origin_of_class(const py::handle& cls)
{
    ScriptOrigin origin;
    origin.language = kLanguage;
    origin.module = cls.attr("__module__").cast<std::string>();

    py::object modules = py::module_::import("sys").attr("modules");
    if (modules.contains(origin.module.c_str()))
    {
        py::object file = py::getattr(modules[origin.module.c_str()], "__file__", py::none());
        if (py::isinstance<py::str>(file))
        {
            origin.source_file = file.cast<std::string>();
        }
    }
    return origin;
}

ParamSchema schema_of_class(const py::handle& cls)
{
    std::string owner = class_name(cls);
    py::list mro = py::list(cls.attr("__mro__"));
    mro.attr("reverse")();

    ParamSchema schema;
    for (const py::handle& base : mro)
    {
        if (is_marker_or_object(base))
        {
            continue;
        }

        py::object annotations = base.attr("__dict__").attr("get")("__annotations__", py::none());
        if (annotations.is_none())
        {
            continue;
        }

        for (const auto& item : annotations.cast<py::dict>())
        {
            std::string field = py::str(item.first);
            py::object annotation = py::reinterpret_borrow<py::object>(item.second);
            if (is_class_var(annotation))
            {
                continue;
            }
            if (field == "name" || field == "num_players")
            {
                throw ScriptError(owner + "." + field + " is reserved and cannot be a parameter; assign it without an annotation");
            }

            ParamType type = param_type_of_annotation(owner, field, annotation);
            if (py::hasattr(cls, field.c_str()))
            {
                add_or_replace(schema, spec_with_default(owner, field, type, cls.attr(field.c_str())));
            }
            else
            {
                add_or_replace(schema, param_without_default(field, type));
            }
        }
    }
    return schema;
}

PyScriptedState::PyScriptedState(py::object state, SharedPtr<const ScriptOrigin> origin, size_t player_count)
    : m_state(std::move(state))
    , m_origin(std::move(origin))
    , m_player_count(player_count)
{
}

ActionList PyScriptedState::legal_actions() const
{
    py::gil_scoped_acquire gil;
    return call_script("state.legal_actions()", [&]
    {
        py::object actions = m_state.get().attr("legal_actions")();
        ActionList result;
        for (const py::handle& action : actions)
        {
            result.push_back(action.cast<ActionId>());
        }
        return result;
    });
}

void PyScriptedState::apply(ActionId action)
{
    py::gil_scoped_acquire gil;
    call_script("state.apply()", [&] { m_state.get().attr("apply")(action); });
}

void PyScriptedState::undo(ActionId action)
{
    py::gil_scoped_acquire gil;
    call_script("state.undo()", [&] { m_state.get().attr("undo")(action); });
}

PlayerId PyScriptedState::current_player() const
{
    py::gil_scoped_acquire gil;
    return call_script("state.current_player()", [&] { return m_state.get().attr("current_player")().cast<PlayerId>(); });
}

bool PyScriptedState::is_terminal() const
{
    py::gil_scoped_acquire gil;
    return call_script("state.is_terminal()", [&] { return m_state.get().attr("is_terminal")().cast<bool>(); });
}

Outcome PyScriptedState::outcome() const
{
    py::gil_scoped_acquire gil;
    return call_script("state.outcome()", [&]
    {
        std::vector<double> values = m_state.get().attr("outcome")().cast<std::vector<double>>();
        if (values.size() != m_player_count)
        {
            throw ScriptError("state.outcome(): expected " + std::to_string(m_player_count) + " rewards but got " + std::to_string(values.size()));
        }

        Outcome result;
        result.is_terminal = m_state.get().attr("is_terminal")().cast<bool>();
        result.rewards = Rewards<double>(m_player_count);
        for (size_t player = 0; player < m_player_count; ++player)
        {
            result.rewards[static_cast<PlayerId>(player)] = values[player];
        }
        return result;
    });
}

std::string PyScriptedState::action_to_string(ActionId action) const
{
    py::gil_scoped_acquire gil;
    return call_script("state.action_to_string()", [&] { return m_state.get().attr("action_to_string")(action).cast<std::string>(); });
}

PyScriptedGame::PyScriptedGame(py::object game, SharedPtr<const ScriptOrigin> origin, ParamSchema schema)
    : m_game(std::move(game))
    , m_origin(std::move(origin))
    , m_schema(std::move(schema))
    , m_num_players(0)
{
    py::gil_scoped_acquire gil;
    const py::object& instance = m_game.get();
    std::string owner = class_name(py::type::of(instance));

    call_script(owner + ".num_players", [&]
    {
        if (!py::hasattr(instance, "num_players"))
        {
            throw ScriptError(owner + " must define num_players, e.g. `num_players = 2`");
        }
        m_num_players = instance.attr("num_players").cast<int32_t>();
        if (m_num_players < 1)
        {
            throw ScriptError(owner + ".num_players must be at least 1");
        }
        m_name = py::hasattr(instance, "name") ? instance.attr("name").cast<std::string>() : owner;
    });
}

UniquePtr<IState> PyScriptedGame::new_initial_state() const
{
    py::gil_scoped_acquire gil;
    return call_script("game.new_initial_state()", [&]
    {
        py::object state = m_game.get().attr("new_initial_state")();
        return UniquePtr<IState>(create_unique<PyScriptedState>(std::move(state), m_origin, static_cast<size_t>(m_num_players)));
    });
}

PyContext::PyContext(const Context& context)
    : m_state(create_shared<PyState>(context.state()))
    , m_features(context.get<IActionFeatures>())
{
}

SharedPtr<PyState> PyContext::state() const
{
    if (!m_valid)
    {
        throw Error("this context is no longer valid: it was only lent to the strategy for the duration of decide()");
    }
    return m_state;
}

IActionFeatures* PyContext::action_features() const
{
    if (!m_valid)
    {
        throw Error("this context is no longer valid: it was only lent to the strategy for the duration of decide()");
    }
    return m_features;
}

void PyContext::invalidate()
{
    m_valid = false;
    m_state->invalidate();
}

PyScriptedStrategy::PyScriptedStrategy(py::object strategy, SharedPtr<const ScriptOrigin> origin)
    : m_strategy(std::move(strategy))
    , m_origin(std::move(origin))
{
}

ActionId PyScriptedStrategy::decide(const Context& context)
{
    py::gil_scoped_acquire gil;

    SharedPtr<PyContext> lent = create_shared<PyContext>(context);
    ActionId action = call_script("strategy.decide()", [&]
    {
        struct Lend
        {
            PyContext& handle;
            ~Lend() { handle.invalidate(); }
        } lend{ *lent };

        return m_strategy.get().attr("decide")(lent).cast<ActionId>();
    });

    if (is_valid(action))
    {
        try
        {
            require_legal(context.state(), action);
        }
        catch (const Error& error)
        {
            throw ScriptError("strategy.decide() returned an illegal action: " + std::string(error.what()));
        }
    }
    return action;
}

SharedPtr<IGame> adapt_game(const py::object& instance)
{
    return create_shared<PyScriptedGame>(instance, shared_origin_of(instance), schema_of_class(py::type::of(instance)));
}

SharedPtr<IStrategy> adapt_strategy(const py::object& instance)
{
    return create_shared<PyScriptedStrategy>(instance, shared_origin_of(instance));
}

bool is_script_game(const py::handle& value)
{
    return py::isinstance(value, marker("Game"));
}

bool is_script_strategy(const py::handle& value)
{
    return py::isinstance(value, marker("Strategy"));
}

} // namespace oryx::python
