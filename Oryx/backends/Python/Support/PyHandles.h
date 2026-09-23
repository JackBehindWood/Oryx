#pragma once

#include "Interop/PyHolder.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Game/IActionFeatures.h"
#include "Oryx/Game/IState.h"
#include "Oryx/Scripting/Support/ScriptLease.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx::python
{

enum class StateAccess
{
    ReadWrite,
    ReadOnly
};

// Python-visible IState: owns its state, or borrows one, usable only while its lease (if any) is armed.
class PyState
{
public:
    explicit PyState(UniquePtr<IState> owned);
    explicit PyState(IState& borrowed, SharedPtr<ScriptLease> lease = nullptr, StateAccess access = StateAccess::ReadWrite);

    [[nodiscard]] IState& get() const;

    [[nodiscard]] std::vector<ActionId> legal_actions() const;
    void apply(ActionId action) const;
    void undo(ActionId action) const;
    [[nodiscard]] std::vector<double> outcome() const;

private:
    void require_writable() const;

    UniquePtr<IState> m_owned;
    IState* m_state;
    SharedPtr<ScriptLease> m_lease;
    StateAccess m_access = StateAccess::ReadWrite;
};

// Python-visible IActionFeatures: game-owned, so usable only while its lease is armed.
class PyActionFeatures
{
public:
    PyActionFeatures(const IActionFeatures& features, SharedPtr<ScriptLease> lease)
        : m_features(features)
        , m_lease(std::move(lease))
    {
    }

    [[nodiscard]] std::vector<int32_t> decode(ActionId action) const;

private:
    const IActionFeatures& m_features;
    SharedPtr<ScriptLease> m_lease;
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

// What a Python strategy's decide() receives: valid only until decide() returns.
class PyContext
{
public:
    PyContext(const Context& context, SharedPtr<ScriptLease> lease);

    [[nodiscard]] SharedPtr<PyState> state() const;
    [[nodiscard]] SharedPtr<PyActionFeatures> action_features() const;

private:
    SharedPtr<PyState> m_state;
    SharedPtr<PyActionFeatures> m_features;
    SharedPtr<ScriptLease> m_lease;
};

} // namespace oryx::python
