#include "OasisLayer.h"

#include <algorithm>
#include <iostream>
#include <typeindex>

namespace oasis
{

OasisLayer::OasisLayer(std::string opponent_arg)
    : oryx::Layer("OasisLayer")
    , m_opponent_arg(std::move(opponent_arg))
{
}

bool OasisLayer::prompt_for_opponent(std::string& out_name) const
{
    std::vector<std::string> names = oryx::StrategyRegistry::names();
    std::sort(names.begin(), names.end());

    while (true)
    {
        std::cout << "Choose an opponent - human";
        for (const std::string& name : names)
        {
            std::cout << ", " << name;
        }
        std::cout << ": ";

        std::string line;
        if (!std::getline(std::cin, line))
        {
            return false;
        }

        if (line == "human" || std::find(names.begin(), names.end(), line) != names.end())
        {
            out_name = line;
            return true;
        }

        std::cout << "Unrecognized choice. Try again.\n";
    }
}

void OasisLayer::attach()
{
    m_game = oryx::GameRegistry::create("tictactoe");
    if (!m_game)
    {
        OX_ERROR("Failed to create game 'tictactoe' — is it registered?");
        oryx::Application::Get().close();
        return;
    }

    m_state = m_game->new_initial_state();

    std::string opponent_name = m_opponent_arg;
    if (opponent_name.empty())
    {
        if (!prompt_for_opponent(opponent_name))
        {
            OX_INFO("Input closed before an opponent was chosen — exiting.");
            oryx::Application::Get().close();
            return;
        }
    }
    else if (opponent_name != "human" && !oryx::StrategyRegistry::has(opponent_name))
    {
        OX_ERROR("Unknown --opponent '{}' — must be 'human' or a registered strategy.", opponent_name);
        oryx::Application::Get().close();
        return;
    }

    if (opponent_name != "human")
    {
        m_opponent = oryx::StrategyRegistry::create(opponent_name);

        Context validation_context(*m_state);
        std::vector<std::type_index> missing;
        for (const std::type_index& capability : m_opponent->required_capabilities())
        {
            if (!validation_context.has_capability(capability))
            {
                missing.push_back(capability);
            }
        }
        if (!missing.empty())
        {
            OX_ERROR("Strategy '{}' requires {} capability(-ies) that game '{}' does not provide.",
                      opponent_name, missing.size(), m_game->name());
            oryx::Application::Get().close();
            return;
        }

        oryx::Random random;
        m_human_player = random.get_bool() ? 0 : 1;
        OX_INFO("You are playing {} against '{}'.", m_human_player == 0 ? "X" : "O", opponent_name);
    }
}

void OasisLayer::update()
{
    if (!m_state)
    {
        return;
    }

    TicTacToeState& state = static_cast<TicTacToeState&>(*m_state);
    m_board.print(state);

    if (state.is_terminal())
    {
        m_board.print_outcome(state.outcome());
        oryx::Application::Get().close();
        return;
    }

    bool human_turn = !m_opponent || state.current_player() == m_human_player;

    ActionId action = oryx::INVALID_ACTION;
    if (human_turn)
    {
        action = m_board.read_move(state);

        if (!oryx::is_valid(action))
        {
            OX_INFO("Input closed before the game finished — exiting.");
            oryx::Application::Get().close();
            return;
        }

        if (action == UNDO_ACTION)
        {
            if (m_history.empty())
            {
                OX_WARN("No moves to undo.");
                return;
            }
            ActionId last_action = m_history.back();
            m_history.pop_back();
            state.undo(last_action);
            return;
        }
    }
    else
    {
        Context context(state);
        action = m_opponent->decide(context);
        OX_INFO("Opponent plays {}.", state.action_to_string(action));
    }

    state.apply(action);
    m_history.push_back(action);
}

void OasisLayer::detach()
{
}

} // namespace oasis
