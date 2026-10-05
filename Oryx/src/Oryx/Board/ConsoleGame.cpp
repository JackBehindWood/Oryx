#include "ConsoleGame.h"

#include <iostream>

namespace oryx
{

ActionId read_console_move(const IState& state)
{
    if (!std::cin)
    {
        return PENDING_ACTION;
    }

    ActionList legal = state.legal_actions();

    std::cout << "Player " << state.current_player() + 1 << " to move:\n";
    for (ActionId action : legal)
    {
        std::cout << "  " << action << ") " << state.action_to_string(action) << "\n";
    }

    while (true)
    {
        std::cout << "Enter a move number or 'u' to undo: ";

        std::string line;
        if (!std::getline(std::cin, line))
        {
            return PENDING_ACTION;
        }

        if (line == "u" || line == "undo")
        {
            return UNDO_ACTION;
        }

        try
        {
            ActionId action = static_cast<ActionId>(std::stoul(line));
            if (std::find(legal.begin(), legal.end(), action) != legal.end())
            {
                return action;
            }
        }
        catch (const std::exception&) {}

        std::cout << "That isn't a legal move. Try again.\n";
    }
}

void print_console_outcome(const Outcome& outcome)
{
    int32_t winner = winner_of(outcome);
    if (winner < 0)
    {
        std::cout << "It's a draw!\n";
    }
    else
    {
        std::cout << "Player " << winner + 1 << " wins!\n";
    }
}

AnnouncingStrategy::AnnouncingStrategy(UniquePtr<IStrategy> inner, std::string name)
    : m_inner(std::move(inner))
    , m_name(std::move(name))
{
}

ActionId AnnouncingStrategy::decide(const Context& context)
{
    ActionId action = m_inner->decide(context);
    if (is_valid(action))
    {
        std::cout << "'" << m_name << "' plays: " << context.state().action_to_string(action) << "\n";
    }
    return action;
}

} // namespace oryx
