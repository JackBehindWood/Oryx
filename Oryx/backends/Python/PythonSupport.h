#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Oryx/Core/Params.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Game/IState.h"
#include "Oryx/Scripting/ScriptError.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx::python
{

// Owns a Python object from C++: released under the GIL, or leaked once the interpreter has been finalised.
class ScriptObject
{
public:
    ScriptObject() = default;
    explicit ScriptObject(pybind11::object object)
        : m_object(std::move(object))
    {
    }
    ~ScriptObject();

    ScriptObject(const ScriptObject&) = delete;
    ScriptObject& operator=(const ScriptObject&) = delete;

    [[nodiscard]] const pybind11::object& get() const { return m_object; }

private:
    pybind11::object m_object;
};

// Python-visible IState: owns its state, or borrows one until invalidate() (see Context handles).
class PyState
{
public:
    explicit PyState(UniquePtr<IState> owned);
    explicit PyState(IState& borrowed);

    [[nodiscard]] IState& get() const;
    void invalidate() { m_state = nullptr; }

    [[nodiscard]] std::vector<ActionId> legal_actions() const;
    void apply(ActionId action) const;
    void undo(ActionId action) const;
    [[nodiscard]] std::vector<double> outcome() const;

private:
    UniquePtr<IState> m_owned;
    IState* m_state;
};

// Python-visible IGame and IStrategy: the binding boundary, so the engine interfaces themselves are never bound.
class PyGame
{
public:
    explicit PyGame(SharedPtr<IGame> game)
        : m_game(std::move(game))
    {
    }

    [[nodiscard]] const SharedPtr<IGame>& get() const { return m_game; }

private:
    SharedPtr<IGame> m_game;
};

class PyStrategy
{
public:
    explicit PyStrategy(SharedPtr<IStrategy> strategy)
        : m_strategy(std::move(strategy))
    {
    }

    [[nodiscard]] const SharedPtr<IStrategy>& get() const { return m_strategy; }

private:
    SharedPtr<IStrategy> m_strategy;
};

void require_legal(const IState& state, ActionId action);

[[nodiscard]] ScriptError to_script_error(const pybind11::error_already_set& error, const std::string& context);

[[nodiscard]] ParamValue to_param_value(const std::string& entry, const std::string& key, const pybind11::handle& value);
[[nodiscard]] Params to_params(const std::string& entry, const pybind11::kwargs& kwargs);
[[nodiscard]] pybind11::object to_python(const ParamValue& value);
[[nodiscard]] pybind11::list schema_to_python(const ParamSchema& schema);

[[nodiscard]] std::vector<double> rewards_to_vector(const Rewards<double>& rewards);

// Accepts a registry name, or a native/Python-defined game.
[[nodiscard]] SharedPtr<IGame> resolve_game(const pybind11::object& spec);
// A registry name is created with `name_params`.
[[nodiscard]] SharedPtr<IStrategy> resolve_strategy(const pybind11::object& spec, const Params& name_params = {});

// True when any participant is backed by a script, so the GIL must stay held.
[[nodiscard]] bool involves_script(const IGame& game, std::span<IStrategy* const> strategies);

} // namespace oryx::python
