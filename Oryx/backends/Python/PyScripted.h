#pragma once

#include "PythonSupport.h"

#include "Oryx/Scripting/IScriptedGame.h"
#include "Oryx/Scripting/IScriptedState.h"
#include "Oryx/Scripting/IScriptedStrategy.h"

namespace oryx::python
{

// Names a Python class's home: its module and, when it has one, the file that module was loaded from.
[[nodiscard]] ScriptOrigin origin_of_class(const pybind11::handle& cls);

// Typed class fields (bool/int/float/str) become parameters; a class value is the default. Throws ScriptError for anything else.
[[nodiscard]] ParamSchema schema_of_class(const pybind11::handle& cls);

class PyScriptedState : public IScriptedState
{
public:
    PyScriptedState(pybind11::object state, SharedPtr<const ScriptOrigin> origin, size_t player_count);

    ActionList legal_actions() const override;
    void apply(ActionId action) override;
    void undo(ActionId action) override;
    PlayerId current_player() const override;
    bool is_terminal() const override;
    Outcome outcome() const override;
    std::string action_to_string(ActionId action) const override;

    const ScriptOrigin& origin() const override { return *m_origin; }

private:
    ScriptObject m_state;
    SharedPtr<const ScriptOrigin> m_origin;
    size_t m_player_count;
};

class PyScriptedGame : public IScriptedGame
{
public:
    PyScriptedGame(pybind11::object game, SharedPtr<const ScriptOrigin> origin, ParamSchema schema);

    UniquePtr<IState> new_initial_state() const override;
    std::string name() const override { return m_name; }
    int32_t num_players() const override { return m_num_players; }

    const ScriptOrigin& origin() const override { return *m_origin; }
    const ParamSchema& param_schema() const override { return m_schema; }

private:
    ScriptObject m_game;
    SharedPtr<const ScriptOrigin> m_origin;
    ParamSchema m_schema;
    std::string m_name;
    int32_t m_num_players;
};

// What a Python strategy's decide() receives: valid only until decide() returns.
class PyContext
{
public:
    explicit PyContext(const Context& context);

    [[nodiscard]] SharedPtr<PyState> state() const;
    [[nodiscard]] IActionFeatures* action_features() const;
    void invalidate();

private:
    SharedPtr<PyState> m_state;
    IActionFeatures* m_features;
    bool m_valid = true;
};

class PyScriptedStrategy : public IScriptedStrategy
{
public:
    PyScriptedStrategy(pybind11::object strategy, SharedPtr<const ScriptOrigin> origin);

    ActionId decide(const Context& context) override;

    const ScriptOrigin& origin() const override { return *m_origin; }

private:
    ScriptObject m_strategy;
    SharedPtr<const ScriptOrigin> m_origin;
};

// Wrap an instance of a class deriving from oryx.Game / oryx.Strategy.
[[nodiscard]] SharedPtr<IGame> adapt_game(const pybind11::object& instance);
[[nodiscard]] SharedPtr<IStrategy> adapt_strategy(const pybind11::object& instance);

[[nodiscard]] bool is_script_game(const pybind11::handle& value);
[[nodiscard]] bool is_script_strategy(const pybind11::handle& value);

} // namespace oryx::python
