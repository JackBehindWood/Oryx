#include "SimulationLayer.h"

#include "Oryx/Core/Application.h"
#include "Oryx/Debug/Instrumentation.h"
#include "Oryx/Events/SimulationEvent.h"

namespace oryx
{

SimulationLayer::SimulationLayer(bool benchmark)
    : Layer("SimulationLayer")
    , m_benchmark(benchmark)
{
}

void SimulationLayer::event(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<StartSimulationEvent>(OX_BIND_EVENT_FN(on_start_simulation));
}

bool SimulationLayer::on_start_simulation(StartSimulationEvent& event)
{
    m_game = event.take_game();
    m_strategy_storage = event.take_strategies();
    m_match_count = event.match_count();
    m_on_turn = event.take_on_turn();

    m_strategies.reserve(m_strategy_storage.size());
    for (const UniquePtr<IStrategy>& strategy : m_strategy_storage)
    {
        m_strategies.push_back(strategy.get());
    }

    m_result.wins.assign(static_cast<size_t>(m_game->num_players()), 0);
    m_result.rewards = Rewards<double>(static_cast<size_t>(m_game->num_players()));

    UniquePtr<IState> probe_state = m_game->new_initial_state();
    for (size_t player = 0; player < m_strategies.size(); ++player)
    {
        Context context = Match::build_context(*m_game, *probe_state);
        std::vector<std::type_index> missing = Match::missing_capabilities(*m_strategies[player], context);
        if (!missing.empty())
        {
            OX_CORE_ERROR("SimulationLayer: player {}'s strategy requires {} capability(-ies) that game '{}' does not provide.",
                           player, missing.size(), m_game->name());
            m_game.reset();
            Application::Get().close();
            return true;
        }
    }
    probe_state.reset();

    if (m_benchmark)
    {
        Instrumentation::reset();
        m_memory_before = MemoryTracker::begin_measurement();
        m_timer.start();
    }

    return true;
}

void SimulationLayer::update()
{
    if (!m_game)
    {
        return; // StartSimulationEvent hasn't set this layer up yet (or setup failed).
    }

    if (!m_match)
    {
        if (m_completed >= m_match_count)
        {
            // close() is unconditional: not every front-end handles this event, and update() would otherwise re-post it every tick.
            if (m_benchmark)
            {
                m_timer.stop();
            }
            MemoryBenchmarkRunner::MemoryResults results;
            results.outcome = m_result;
            if (m_benchmark)
            {
                results.elapsed_seconds = m_timer.elapsed_seconds();
                results.memory = memory_delta(m_memory_before, MemoryTracker::snapshot());
            }
            SimulationCompleteEvent event(std::move(results), m_benchmark);
            Application::Get().post_event(event);
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
        m_result.decisions += static_cast<int64_t>(m_match->history().size());
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
