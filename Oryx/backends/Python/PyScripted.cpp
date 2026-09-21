#include "oxpch.h"
#include "PyScripted.h"

#include <pybind11/pybind11.h>

#include "Support/PyHandles.h"
#include "Oryx/Scripting/Support/ScriptUtil.h"

namespace py = pybind11;

namespace oryx::python
{

PyScriptedState::PyScriptedState(PyRef state, SharedPtr<const ScriptOrigin> origin, size_t player_count)
    : PyAdapter(std::move(state), std::move(origin))
    , m_player_count(player_count)
{
}

ActionList PyScriptedState::legal_actions() const
{
    if (!m_legal_actions_valid)
    {
        m_legal_actions = call<"legal_actions", ActionList>();
        m_legal_actions_valid = true;
    }
    return m_legal_actions;
}

void PyScriptedState::apply(ActionId action)
{
    m_legal_actions_valid = false;
    call<"apply">(action);
}

void PyScriptedState::undo(ActionId action)
{
    m_legal_actions_valid = false;
    call<"undo">(action);
}

PlayerId PyScriptedState::current_player() const
{
    return call<"current_player", PlayerId>();
}

bool PyScriptedState::is_terminal() const
{
    return call<"is_terminal", bool>();
}

Outcome PyScriptedState::outcome() const
{
    std::vector<double> rewards = call<"outcome", std::vector<double>>();
    return outcome_from_rewards(rewards, m_player_count, call<"is_terminal", bool>());
}

std::string PyScriptedState::action_to_string(ActionId action) const
{
    return call<"action_to_string", std::string>(action);
}

PyScriptedGame::PyScriptedGame(PyRef game, SharedPtr<const ScriptOrigin> origin, ParamSchema schema)
    : PyAdapter(std::move(game), std::move(origin))
    , m_schema(std::move(schema))
{
    PyGil gil;
    std::string owner = Py_TYPE(m_object.get())->tp_name;
    std::string context = owner + ".num_players";

    PyRef players = PyRef::steal(PyObject_GetAttrString(m_object.get(), "num_players"));
    if (!players)
    {
        PyErr_Clear();
        throw ScriptError(owner + " must define num_players, e.g. `num_players = 2`");
    }
    if (!PyConvert<int32_t>::from_py(players.get(), m_num_players))
    {
        throw_wrong_type(context, PyConvert<int32_t>::expected, players.get());
    }
    if (m_num_players < 1)
    {
        throw ScriptError(owner + ".num_players must be at least 1");
    }

    m_name = owner;
    PyRef name = PyRef::steal(PyObject_GetAttrString(m_object.get(), "name"));
    if (!name)
    {
        PyErr_Clear();
    }
    else if (!PyConvert<std::string>::from_py(name.get(), m_name))
    {
        throw_wrong_type(owner + ".name", PyConvert<std::string>::expected, name.get());
    }
}

UniquePtr<IState> PyScriptedGame::new_initial_state() const
{
    PyRef state = call<"new_initial_state", PyRef>();
    if (state.get() == Py_None)
    {
        PyGil gil;
        throw_wrong_type("game.new_initial_state()", "a state", state.get());
    }
    return create_unique<PyScriptedState>(std::move(state), m_origin, static_cast<size_t>(m_num_players));
}

PyScriptedStrategy::PyScriptedStrategy(PyRef strategy, SharedPtr<const ScriptOrigin> origin)
    : PyAdapter(std::move(strategy), std::move(origin))
{
}

ActionId PyScriptedStrategy::decide(const Context& context)
{
    PyGil gil;

    SharedPtr<ScriptLease> lease = create_shared<ScriptLease>();
    PyRef lent = PyRef::steal(py::cast(create_shared<PyContext>(context, lease)).release().ptr());
    ActionId action = 0;
    {
        LeaseScope scope(*lease);
        action = call<"decide", ActionId>(lent);
    }

    check_legal(context.state(), action, "strategy.decide() returned an illegal action");
    return action;
}

} // namespace oryx::python
