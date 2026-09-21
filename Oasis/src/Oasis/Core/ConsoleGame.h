#pragma once

#include "Oryx.h"

namespace oasis
{

// Text UI for games Oasis has no dedicated renderer for: legal moves are listed with their action_to_string() and chosen by ActionId.
// Returns oryx::UNDO_ACTION for 'u', and oryx::INVALID_ACTION once stdin is closed.
oryx::ActionId read_console_move(const oryx::IState& state);

void print_console_outcome(const oryx::Outcome& outcome);

// A generic game has no board to show what a strategy played, so this says so.
class AnnouncingStrategy : public oryx::IStrategy
{
public:
    AnnouncingStrategy(oryx::UniquePtr<oryx::IStrategy> inner, std::string name);

    oryx::ActionId decide(const oryx::Context& context) override;
    std::vector<std::type_index> required_capabilities() const override { return m_inner->required_capabilities(); }

private:
    oryx::UniquePtr<oryx::IStrategy> m_inner;
    std::string m_name;
};

} // namespace oasis
