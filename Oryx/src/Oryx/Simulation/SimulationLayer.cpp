#include "SimulationLayer.h"

#include "Oryx/Core/Application.h"

namespace oryx
{

SimulationLayer::SimulationLayer(UniquePtr<IGame> game,
                                  std::vector<UniquePtr<IStrategy>> strategies,
                                  int32_t match_count,
                                  TurnObserver on_turn)
    : Layer("SimulationLayer")
    , m_game(std::move(game))
    , m_strategy_storage(std::move(strategies))
    , m_match_count(match_count)
    , m_on_turn(std::move(on_turn))
{
    m_strategies.reserve(m_strategy_storage.size());
    for (const UniquePtr<IStrategy>& strategy : m_strategy_storage)
    {
        m_strategies.push_back(strategy.get());
    }

    m_result.wins.assign(static_cast<size_t>(m_game->num_players()), 0);
    m_result.rewards = Rewards<double>(static_cast<size_t>(m_game->num_players()));
}

void SimulationLayer::attach()
{
    UniquePtr<IState> probe_state = m_game->new_initial_state();
    for (size_t player = 0; player < m_strategies.size(); ++player)
    {
        Context context = Match::build_context(*m_game, *probe_state);
        std::vector<std::type_index> missing = Match::missing_capabilities(*m_strategies[player], context);
        if (!missing.empty())
        {
            OX_CORE_ERROR("SimulationLayer: player {}'s strategy requires {} capability(-ies) that game '{}' does not provide.",
                           player, missing.size(), m_game->name());
            Application::Get().close();
            return;
        }
    }
}

void SimulationLayer::update()
{
    if (!m_match)
    {
        if (m_completed >= m_match_count)
        {
            OX_CORE_INFO("Simulation complete: {} match(es), {} draw(s).", m_result.matches, m_result.draws);
            Application::Get().close();
            return;
        }
        m_match = create_unique<Match>(*m_game, m_strategies);
    }

    if (m_on_turn)
    {
        m_on_turn(m_match->state());
    }

    if (m_match->is_terminal())
    {
        accumulate(m_result, m_match->outcome());
        ++m_completed;
        m_match.reset();
        return;
    }

    ActionId action = m_match->decide();

    if (!is_valid(action))
    {
        OX_CORE_INFO("Input closed before the match finished - exiting.");
        Application::Get().close();
        return;
    }

    if (action == UNDO_ACTION)
    {
        if (!m_match->history().can_undo())
        {
            OX_CORE_WARN("No moves to undo.");
            return;
        }
        m_match->undo();
        return;
    }

    m_match->apply(action);
}

} // namespace oryx
