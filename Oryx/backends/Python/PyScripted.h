#pragma once

#include "Interop/PyMethods.h"

#include "Oryx/Core/Params.h"
#include "Oryx/Scripting/Interfaces/IScriptedGame.h"
#include "Oryx/Scripting/Interfaces/IScriptedState.h"
#include "Oryx/Scripting/Interfaces/IScriptedStrategy.h"

namespace oryx::python
{

// What every adapter shares: the script's object, its class's method table and where it came from.
template<typename Set>
class PyAdapter
{
protected:
    PyAdapter(PyRef object, SharedPtr<const ScriptOrigin> origin)
        : m_object(std::move(object))
        , m_methods(load_methods())
        , m_origin(std::move(origin))
    {
    }

    template<FixedString Name, typename Result = void, typename... Args>
    Result call(const Args&... args) const
    {
        return call_method<Set, Name, Result>(m_object, *m_methods, args...);
    }

    PyScriptObject m_object;
    SharedPtr<const PyClassMethods> m_methods;
    SharedPtr<const ScriptOrigin> m_origin;

private:
    [[nodiscard]] SharedPtr<const PyClassMethods> load_methods() const
    {
        PyGil gil;
        return class_methods_for<Set>(m_object.get());
    }
};

class PyScriptedState : public IScriptedState, private PyAdapter<StateMethods>
{
public:
    PyScriptedState(PyRef state, SharedPtr<const ScriptOrigin> origin, size_t player_count);

    ActionList legal_actions() const override;
    void apply(ActionId action) override;
    void undo(ActionId action) override;
    PlayerId current_player() const override;
    bool is_terminal() const override;
    Outcome outcome() const override;
    std::string action_to_string(ActionId action) const override;

    const ScriptOrigin& origin() const override { return *m_origin; }

private:
    size_t m_player_count;
    // Valid until apply() or undo(): a state changes only through them, and apply() legality-checks the list legal_actions() just returned.
    mutable ActionList m_legal_actions;
    mutable bool m_legal_actions_valid = false;
};

class PyScriptedGame : public IScriptedGame, private PyAdapter<GameMethods>
{
public:
    PyScriptedGame(PyRef game, SharedPtr<const ScriptOrigin> origin, ParamSchema schema);

    UniquePtr<IState> new_initial_state() const override;
    std::string name() const override { return m_name; }
    int32_t num_players() const override { return m_num_players; }

    const ScriptOrigin& origin() const override { return *m_origin; }
    const ParamSchema& param_schema() const override { return m_schema; }

private:
    ParamSchema m_schema;
    std::string m_name;
    int32_t m_num_players = 0;
};

class PyScriptedStrategy : public IScriptedStrategy, private PyAdapter<StrategyMethods>
{
public:
    PyScriptedStrategy(PyRef strategy, SharedPtr<const ScriptOrigin> origin);

    ActionId decide(const Context& context) override;

    const ScriptOrigin& origin() const override { return *m_origin; }
};

} // namespace oryx::python
