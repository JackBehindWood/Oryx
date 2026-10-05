#pragma once

#include "Oryx/Board/IConsoleBoard.h"
#include "Oryx/Game/Outcome.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

// Text UI helpers for games without a dedicated board: legal moves are listed with their action_to_string() and chosen by ActionId.
// Returns UNDO_ACTION for 'u', and PENDING_ACTION once stdin is exhausted (BoardLayer then closes the application).
ActionId read_console_move(const IState& state);

void print_console_outcome(const Outcome& outcome);

// A generic game has no board to show what a strategy played, so this says so.
class AnnouncingStrategy : public IStrategy
{
public:
    AnnouncingStrategy(UniquePtr<IStrategy> inner, std::string name);

    ActionId decide(const Context& context) override;
    std::vector<std::type_index> required_capabilities() const override { return m_inner->required_capabilities(); }

private:
    UniquePtr<IStrategy> m_inner;
    std::string m_name;
};

} // namespace oryx
