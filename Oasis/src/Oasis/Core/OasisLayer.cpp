#include "OasisLayer.h"

#include "Oryx/Benchmark/BenchmarkReport.h"
#include "Oryx/Events/SimulationEvent.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace oasis
{

using oryx::ActionId;
using oryx::Context;
using oryx::ExternalStrategy;
using oryx::IGame;
using oryx::IState;
using oryx::IStrategy;
using oryx::SimulationLayer;
using oryx::UniquePtr;

OasisLayer::OasisLayer(std::string opponent_arg, std::string simulate_arg, bool benchmark_arg)
    : oryx::Layer("OasisLayer")
    , m_opponent_arg(std::move(opponent_arg))
    , m_simulate_arg(std::move(simulate_arg))
    , m_benchmark_arg(benchmark_arg)
{
}

void OasisLayer::event(oryx::Event& event)
{
    oryx::EventDispatcher dispatcher(event);
    dispatcher.dispatch<oryx::SimulationCompleteEvent>(OX_BIND_EVENT_FN(on_simulation_complete));
}

bool OasisLayer::on_simulation_complete(const oryx::SimulationCompleteEvent& event)
{
    // SimulationLayer closes the Application itself right after posting
    // this - we only report here.
    if (event.benchmark())
    {
        std::cout << oryx::format_benchmark_report(event.results());
    }
    else
    {
        OX_CORE_INFO("Simulation complete: {} match(es), {} draw(s).", event.result().matches, event.result().draws);
    }

    return true;
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
    UniquePtr<IGame> game = oryx::create_game("tictactoe");
    if (!game)
    {
        OX_ERROR("Failed to create game 'tictactoe' — is it registered?");
        oryx::Application::Get().close();
        return;
    }

    if (!m_simulate_arg.empty())
    {
        attach_simulate(std::move(game));
        return;
    }

    attach_interactive(std::move(game));
}

void OasisLayer::attach_simulate(UniquePtr<IGame> game)
{
    size_t first_comma = m_simulate_arg.find(',');
    size_t second_comma = first_comma == std::string::npos ? std::string::npos : m_simulate_arg.find(',', first_comma + 1);
    if (first_comma == std::string::npos || second_comma == std::string::npos)
    {
        OX_ERROR("--simulate expects <strategyA>,<strategyB>,<matchCount>, got '{}'.", m_simulate_arg);
        oryx::Application::Get().close();
        return;
    }

    std::string strategy_a_name = m_simulate_arg.substr(0, first_comma);
    std::string strategy_b_name = m_simulate_arg.substr(first_comma + 1, second_comma - first_comma - 1);
    std::string count_string = m_simulate_arg.substr(second_comma + 1);

    if (!oryx::StrategyRegistry::has(strategy_a_name) || !oryx::StrategyRegistry::has(strategy_b_name))
    {
        OX_ERROR("--simulate: unknown strategy name(s) '{}', '{}'.", strategy_a_name, strategy_b_name);
        oryx::Application::Get().close();
        return;
    }

    int32_t match_count = 0;
    try
    {
        match_count = std::stoi(count_string);
    }
    catch (const std::exception&)
    {
        OX_ERROR("--simulate: invalid match count '{}'.", count_string);
        oryx::Application::Get().close();
        return;
    }

    oryx::SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(oryx::StrategyRegistry::create(strategy_a_name));
    strategies.push_back(oryx::StrategyRegistry::create(strategy_b_name));

    oryx::StartSimulationEvent event(std::move(game), std::move(strategies), match_count, /*on_turn=*/nullptr, m_benchmark_arg);
    oryx::Application::Get().post_event(event);
}

void OasisLayer::attach_interactive(UniquePtr<IGame> game)
{
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

    ExternalStrategy::InputProvider human_input = [this](const Context& context) -> ActionId
    {
        return m_board.read_move(static_cast<const TicTacToeState&>(context.state()));
    };

    oryx::SmallVector<UniquePtr<IStrategy>, 2> strategies(2);

    if (opponent_name == "human")
    {
        strategies[0] = oryx::create_unique<ExternalStrategy>(human_input);
        strategies[1] = oryx::create_unique<ExternalStrategy>(human_input);
        OX_INFO("Human vs human.");
    }
    else
    {
        UniquePtr<IStrategy> opponent = oryx::StrategyRegistry::create(opponent_name);

        oryx::Random random;
        int32_t human_player = random.get_bool() ? 0 : 1;
        strategies[static_cast<size_t>(human_player)] = oryx::create_unique<ExternalStrategy>(human_input);
        strategies[static_cast<size_t>(1 - human_player)] = std::move(opponent);
        OX_INFO("You are playing {} against '{}'.", human_player == 0 ? "X" : "O", opponent_name);
    }

    SimulationLayer::TurnObserver render = [this](IState& state)
    {
        TicTacToeState& tic_tac_toe_state = static_cast<TicTacToeState&>(state);
        m_board.print(tic_tac_toe_state);
        if (tic_tac_toe_state.is_terminal())
        {
            m_board.print_outcome(tic_tac_toe_state.outcome());
        }
    };

    oryx::StartSimulationEvent event(std::move(game), std::move(strategies), /*match_count=*/1, render, m_benchmark_arg);
    oryx::Application::Get().post_event(event);
}

} // namespace oasis
