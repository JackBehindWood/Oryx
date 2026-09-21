#include "oxpch.h"
#include "PythonSupport.h"

#include "PyScripted.h"
#include "Oryx/Scripting/IScriptedGame.h"
#include "Oryx/Scripting/IScriptedStrategy.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

std::string type_name_of(const py::handle& value)
{
    return py::str(py::type::of(value).attr("__name__"));
}

} // namespace

ScriptObject::~ScriptObject()
{
    if (!m_object)
    {
        return;
    }

    if (Py_IsInitialized() != 0)
    {
        py::gil_scoped_acquire gil;
        m_object = py::object();
    }
    else
    {
        m_object.release();
    }
}

PyState::PyState(UniquePtr<IState> owned)
    : m_owned(std::move(owned))
    , m_state(m_owned.get())
{
}

PyState::PyState(IState& borrowed)
    : m_state(&borrowed)
{
}

IState& PyState::get() const
{
    if (m_state == nullptr)
    {
        throw Error("this state is no longer valid: it was only lent to the strategy for the duration of decide()");
    }
    return *m_state;
}

std::vector<ActionId> PyState::legal_actions() const
{
    ActionList actions = get().legal_actions();
    return std::vector<ActionId>(actions.begin(), actions.end());
}

void PyState::apply(ActionId action) const
{
    IState& state = get();
    require_legal(state, action);
    state.apply(action);
}

void PyState::undo(ActionId action) const
{
    get().undo(action);
}

std::vector<double> PyState::outcome() const
{
    return rewards_to_vector(get().outcome().rewards);
}

void require_legal(const IState& state, ActionId action)
{
    ActionList legal = state.legal_actions();
    if (std::find(legal.begin(), legal.end(), action) == legal.end())
    {
        throw Error("action " + to_string(action) + " is not legal in this state");
    }
}

ScriptError to_script_error(const py::error_already_set& error, const std::string& context)
{
    std::string detail = error.what();
    return ScriptError(context + ": " + detail.substr(0, detail.find('\n')), detail);
}

ParamValue to_param_value(const std::string& entry, const std::string& key, const py::handle& value)
{
    if (py::isinstance<py::bool_>(value))
    {
        return value.cast<bool>();
    }
    if (py::isinstance<py::int_>(value))
    {
        return value.cast<int64_t>();
    }
    if (py::isinstance<py::float_>(value))
    {
        return value.cast<double>();
    }
    if (py::isinstance<py::str>(value))
    {
        return value.cast<std::string>();
    }
    throw ParamError(entry, key, "has unsupported type '" + type_name_of(value) + "' (expected bool, int, float or str)");
}

Params to_params(const std::string& entry, const py::kwargs& kwargs)
{
    Params params;
    for (const auto& item : kwargs)
    {
        std::string key = py::str(item.first);
        params.emplace(key, to_param_value(entry, key, item.second));
    }
    return params;
}

py::object to_python(const ParamValue& value)
{
    return std::visit([](const auto& alternative) { return py::cast(alternative); }, value);
}

py::list schema_to_python(const ParamSchema& schema)
{
    py::list result;
    for (const ParamSpec& spec : schema)
    {
        py::dict entry;
        entry["name"] = spec.name;
        entry["type"] = to_string(spec.type);
        entry["description"] = spec.description;
        if (spec.has_default)
        {
            entry["default"] = to_python(spec.default_value);
        }
        result.append(entry);
    }
    return result;
}

std::vector<double> rewards_to_vector(const Rewards<double>& rewards)
{
    std::vector<double> result;
    result.reserve(rewards.player_count());
    for (size_t player = 0; player < rewards.player_count(); ++player)
    {
        result.push_back(rewards[static_cast<PlayerId>(player)]);
    }
    return result;
}

SharedPtr<IGame> resolve_game(const py::object& spec)
{
    if (py::isinstance<py::str>(spec))
    {
        std::string name = spec.cast<std::string>();
        SharedPtr<IGame> game = create_game(name);
        if (!game)
        {
            throw Error("no game is registered as '" + name + "'");
        }
        return game;
    }
    if (py::isinstance<PyGame>(spec))
    {
        return spec.cast<const PyGame&>().get();
    }
    if (is_script_game(spec))
    {
        return adapt_game(spec);
    }
    throw Error("expected a game name or a game, got '" + type_name_of(spec) + "'");
}

SharedPtr<IStrategy> resolve_strategy(const py::object& spec, const Params& name_params)
{
    if (py::isinstance<py::str>(spec))
    {
        std::string name = spec.cast<std::string>();
        SharedPtr<IStrategy> strategy = StrategyRegistry::create(name, name_params);
        if (!strategy)
        {
            throw Error("no strategy is registered as '" + name + "'");
        }
        return strategy;
    }
    if (py::isinstance<PyStrategy>(spec))
    {
        return spec.cast<const PyStrategy&>().get();
    }
    if (is_script_strategy(spec))
    {
        return adapt_strategy(spec);
    }
    throw Error("expected a strategy name or a strategy, got '" + type_name_of(spec) + "'");
}

bool involves_script(const IGame& game, std::span<IStrategy* const> strategies)
{
    if (dynamic_cast<const IScriptedGame*>(&game) != nullptr)
    {
        return true;
    }
    return std::any_of(strategies.begin(), strategies.end(), [](const IStrategy* strategy) { return dynamic_cast<const IScriptedStrategy*>(strategy) != nullptr; });
}

} // namespace oryx::python
