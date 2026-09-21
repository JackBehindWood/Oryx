#include "ConsoleGame.h"

namespace oasis
{

oryx::ActionId read_console_move(const oryx::IState& state)
{
    oryx::ActionList legal = state.legal_actions();

    std::cout << "Player " << state.current_player() + 1 << " to move:\n";
    for (oryx::ActionId action : legal)
    {
        std::cout << "  " << action << ") " << state.action_to_string(action) << "\n";
    }

    while (true)
    {
        std::cout << "Enter a move number or 'u' to undo: ";

        std::string line;
        if (!std::getline(std::cin, line))
        {
            return oryx::INVALID_ACTION;
        }

        if (line == "u" || line == "undo")
        {
            return oryx::UNDO_ACTION;
        }

        try
        {
            oryx::ActionId action = static_cast<oryx::ActionId>(std::stoul(line));
            if (std::find(legal.begin(), legal.end(), action) != legal.end())
            {
                return action;
            }
        }
        catch (const std::exception&) {}

        std::cout << "That isn't a legal move. Try again.\n";
    }
}

void print_console_outcome(const oryx::Outcome& outcome)
{
    size_t best = 0;
    bool tie = false;
    for (size_t player = 1; player < outcome.rewards.player_count(); ++player)
    {
        double reward = outcome.rewards[static_cast<oryx::PlayerId>(player)];
        double best_reward = outcome.rewards[static_cast<oryx::PlayerId>(best)];
        if (reward > best_reward)
        {
            best = player;
            tie = false;
        }
        else if (reward == best_reward)
        {
            tie = true;
        }
    }

    if (tie)
    {
        std::cout << "It's a draw!\n";
    }
    else
    {
        std::cout << "Player " << best + 1 << " wins!\n";
    }
}

AnnouncingStrategy::AnnouncingStrategy(oryx::UniquePtr<oryx::IStrategy> inner, std::string name)
    : m_inner(std::move(inner))
    , m_name(std::move(name))
{
}

oryx::ActionId AnnouncingStrategy::decide(const oryx::Context& context)
{
    oryx::ActionId action = m_inner->decide(context);
    if (oryx::is_valid(action))
    {
        std::cout << "'" << m_name << "' plays: " << context.state().action_to_string(action) << "\n";
    }
    return action;
}

} // namespace oasis
