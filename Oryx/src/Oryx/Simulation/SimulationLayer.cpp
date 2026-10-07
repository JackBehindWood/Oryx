#include "SimulationLayer.h"

#include "Oryx/Core/Application.h"
#include "Oryx/Debug/Instrumentation.h"
#include "Oryx/Memory/DefaultAllocator.h"
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
    dispatcher.dispatch<RestartSimulationEvent>(OX_BIND_EVENT_FN(on_restart_simulation));
}

bool SimulationLayer::on_restart_simulation(RestartSimulationEvent& event)
{
    m_restart_requested = true;
    m_next_seat_order = event.seat_order();
    return true;
}

void SimulationLayer::apply_seat_order()
{
    if (m_next_seat_order.empty())
    {
        return;
    }
    bool valid = m_next_seat_order.size() == m_strategy_storage.size();
    for (uint32_t index : m_next_seat_order)
    {
        valid = valid && index < m_strategy_storage.size();
    }
    if (valid)
    {
        for (size_t seat = 0; seat < m_next_seat_order.size(); ++seat)
        {
            m_strategies[seat] = m_strategy_storage[m_next_seat_order[seat]].get();
        }
    }
    else
    {
        OX_CORE_WARN("SimulationLayer: ignoring a seat order that does not name each of the {} strategies.", m_strategy_storage.size());
    }
    m_next_seat_order.clear();
}

bool SimulationLayer::on_start_simulation(StartSimulationEvent& event)
{
    m_game = event.take_game();
    m_strategy_storage = event.take_strategies();
    m_match_count = event.match_count();
    m_observer = event.take_observer();
    m_linger = event.linger();

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
        m_memory_before = default_allocator().counters().begin_measurement();
        m_timer.start();
    }

    return true;
}

void SimulationLayer::update(double)
{
    if (!m_game)
    {
        return; // StartSimulationEvent hasn't set this layer up yet (or setup failed).
    }

    if (!m_match)
    {
        if (!m_linger && m_completed >= m_match_count)
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
                results.memory = memory_delta(m_memory_before, default_allocator_stats());
            }
            SimulationCompleteEvent event(std::move(results), m_benchmark);
            Application::Get().post_event(event);
            Application::Get().close();
            return;
        }
        apply_seat_order();
        m_match = create_unique<Match>(*m_game, m_strategies);
        m_restart_requested = false;
        if (m_observer)
        {
            m_observer->on_match_start();
        }
    }

    if (m_observer)
    {
        m_observer->on_turn(m_match->state());
    }

    if (m_match->is_terminal())
    {
        if (m_linger && !m_restart_requested)
        {
            return;
        }
        m_restart_requested = false;
        accumulate(m_result, m_match->outcome());
        m_result.decisions += static_cast<int64_t>(m_match->history().size());
        ++m_completed;
        m_match.reset();
        return;
    }

    ActionId action = m_match->decide();

    if (action == PENDING_ACTION)
    {
        return;
    }

    if (!is_valid(action))
    {
        OX_CORE_ERROR("SimulationLayer: a strategy returned INVALID_ACTION in a non-terminal state of '{}'.", m_game->name());
        Application::Get().close(1);
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

    OX_CORE_ASSERT(is_game_action(action), "SimulationLayer: a strategy returned a reserved action id.");
    m_match->apply(action);
}

} // namespace oryx
