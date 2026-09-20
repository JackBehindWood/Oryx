#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Game/Context.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Game/Outcome.h"
#include "Oryx/Game/PlayerId.h"
#include "Oryx/Simulation/ActionHistory.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

class Match
{
public:
    Match(const IGame& game, SmallVector<IStrategy*, 2> strategies);

    [[nodiscard]] IState& state() const { return *m_state; }
    [[nodiscard]] bool is_terminal() const { return m_state->is_terminal(); }
    [[nodiscard]] PlayerId current_player() const { return m_state->current_player(); }
    [[nodiscard]] Outcome outcome() const { return m_state->outcome(); }

    // Builds the Context, validates capabilities, and asks the current
    // seat's strategy to decide - does not apply the result.
    [[nodiscard]] ActionId decide() const;

    void apply(ActionId action);   // applies to state, records into history
    ActionId undo();               // IState::undo() + history().undo() in lockstep; INVALID_ACTION if nothing to undo
    ActionId redo();               // IState::apply() + history().redo() in lockstep; INVALID_ACTION if nothing to redo

    // Loops apply(decide()) to termination.
    Outcome play();

    [[nodiscard]] const ActionHistory& history() const { return m_history; }

    // Shared with OasisLayer/SimulationLayer so Context construction and
    // capability validation live in exactly one place (docs/architecture.md §14).
    static Context build_context(const IGame& game, IState& state);
    static std::vector<std::type_index> missing_capabilities(const IStrategy& strategy, const Context& context);

private:
    [[nodiscard]] IStrategy& current_strategy() const;

    const IGame& m_game;
    SmallVector<IStrategy*, 2> m_strategies;
    UniquePtr<IState> m_state;
    ActionHistory m_history;
};

} // namespace oryx
