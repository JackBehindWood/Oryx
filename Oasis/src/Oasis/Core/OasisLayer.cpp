#include "OasisLayer.h"

#include "ConsoleGame.h"

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

namespace
{

constexpr const char* kTicTacToe = "tictactoe";

std::vector<std::string> sorted(std::vector<std::string> names)
{
    std::sort(names.begin(), names.end());
    return names;
}

std::string joined(const std::vector<std::string>& names)
{
    std::string result;
    for (const std::string& name : names)
    {
        result += (result.empty() ? "" : ", ") + name;
    }
    return result;
}

// A strategy named "<game>/<name>" only understands that game's states; unprefixed strategies play anything.
bool fits_game(const std::string& strategy, const std::string& game)
{
    size_t slash = strategy.find('/');
    return slash == std::string::npos || strategy.substr(0, slash) == game;
}

std::vector<std::string> strategies_for(const std::string& game)
{
    std::vector<std::string> names;
    for (const std::string& name : oryx::StrategyRegistry::names())
    {
        if (fits_game(name, game))
        {
            names.push_back(name);
        }
    }
    return sorted(std::move(names));
}

bool prompt_for_choice(const std::string& what, const std::vector<std::string>& names, std::string& out_name)
{
    while (true)
    {
        std::cout << "Choose " << what << " - " << joined(names) << ": ";

        std::string line;
        if (!std::getline(std::cin, line))
        {
            return false;
        }

        if (std::find(names.begin(), names.end(), line) != names.end())
        {
            out_name = line;
            return true;
        }

        std::cout << "Unrecognized choice. Try again.\n";
    }
}

UniquePtr<IStrategy> create_opponent(const std::string& name, bool announce)
{
    UniquePtr<IStrategy> opponent = oryx::StrategyRegistry::create(name);
    if (announce)
    {
        return oryx::create_unique<oasis::AnnouncingStrategy>(std::move(opponent), name);
    }
    return opponent;
}

} // namespace

OasisLayer::OasisLayer(std::string game_arg, std::string opponent_arg, std::string simulate_arg, bool benchmark_arg)
    : oryx::Layer("OasisLayer")
    , m_game_arg(std::move(game_arg))
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

bool OasisLayer::choose_game(std::string& out_name) const
{
    std::vector<std::string> names = sorted(oryx::GameRegistry::names());

    if (!m_game_arg.empty())
    {
        if (!oryx::GameRegistry::has(m_game_arg))
        {
            OX_ERROR("Unknown --game '{}' — registered games: {}.", m_game_arg, joined(names));
            return false;
        }
        out_name = m_game_arg;
        return true;
    }

    if (names.empty())
    {
        OX_ERROR("No game is registered.");
        return false;
    }

    // Simulations are non-interactive, so they take the default rather than ask.
    if (names.size() == 1 || !m_simulate_arg.empty())
    {
        out_name = oryx::GameRegistry::has(kTicTacToe) ? kTicTacToe : names.front();
        return true;
    }

    if (!prompt_for_choice("a game", names, out_name))
    {
        OX_INFO("Input closed before a game was chosen — exiting.");
        return false;
    }
    return true;
}

void OasisLayer::attach()
{
    std::string game_name;
    UniquePtr<IGame> game;
    if (choose_game(game_name))
    {
        game = oryx::create_game(game_name);
    }

    if (!game)
    {
        oryx::Application::Get().close();
        return;
    }
    OX_INFO("Playing {}.", game->name());

    if (!m_simulate_arg.empty())
    {
        attach_simulate(game_name, std::move(game));
        return;
    }

    attach_interactive(game_name, std::move(game));
}

void OasisLayer::attach_simulate(const std::string& game_name, UniquePtr<IGame> game)
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

    std::vector<std::string> available = strategies_for(game_name);
    bool known = std::find(available.begin(), available.end(), strategy_a_name) != available.end() &&
                 std::find(available.begin(), available.end(), strategy_b_name) != available.end();
    if (!known)
    {
        OX_ERROR("--simulate: '{}' and '{}' must both be strategies for '{}' ({}).", strategy_a_name, strategy_b_name, game_name, joined(available));
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

    if (game->num_players() != 2)
    {
        OX_ERROR("--simulate needs a two-player game, but '{}' has {} players.", game_name, game->num_players());
        oryx::Application::Get().close();
        return;
    }

    oryx::SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(oryx::StrategyRegistry::create(strategy_a_name));
    strategies.push_back(oryx::StrategyRegistry::create(strategy_b_name));

    oryx::StartSimulationEvent event(std::move(game), std::move(strategies), match_count, /*on_turn=*/nullptr, m_benchmark_arg);
    oryx::Application::Get().post_event(event);
}

void OasisLayer::attach_interactive(const std::string& game_name, UniquePtr<IGame> game)
{
    std::vector<std::string> opponents = strategies_for(game_name);
    opponents.insert(opponents.begin(), "human");

    std::string opponent_name = m_opponent_arg;
    if (opponent_name.empty())
    {
        if (!prompt_for_choice("an opponent", opponents, opponent_name))
        {
            OX_INFO("Input closed before an opponent was chosen — exiting.");
            oryx::Application::Get().close();
            return;
        }
    }
    else if (std::find(opponents.begin(), opponents.end(), opponent_name) == opponents.end())
    {
        OX_ERROR("Unknown --opponent '{}' for '{}' — must be one of: {}.", opponent_name, game_name, joined(opponents));
        oryx::Application::Get().close();
        return;
    }

    bool tic_tac_toe = game_name == kTicTacToe;

    ExternalStrategy::InputProvider human_input = [this, tic_tac_toe](const Context& context) -> ActionId
    {
        if (tic_tac_toe)
        {
            return m_board.read_move(static_cast<const TicTacToeState&>(context.state()));
        }
        return read_console_move(context.state());
    };

    size_t seat_count = static_cast<size_t>(game->num_players());
    oryx::SmallVector<UniquePtr<IStrategy>, 2> strategies(seat_count);

    if (opponent_name == "human")
    {
        for (size_t seat = 0; seat < seat_count; ++seat)
        {
            strategies[seat] = oryx::create_unique<ExternalStrategy>(human_input);
        }
        OX_INFO("Human vs human.");
    }
    else
    {
        oryx::Random random;
        size_t human_seat = static_cast<size_t>(random.get_int(0, static_cast<int64_t>(seat_count) - 1));
        for (size_t seat = 0; seat < seat_count; ++seat)
        {
            strategies[seat] = seat == human_seat ? oryx::create_unique<ExternalStrategy>(human_input) : create_opponent(opponent_name, !tic_tac_toe);
        }

        if (tic_tac_toe)
        {
            OX_INFO("You are playing {} against '{}'.", human_seat == 0 ? "X" : "O", opponent_name);
        }
        else
        {
            OX_INFO("You are player {} against '{}'.", human_seat + 1, opponent_name);
        }
    }

    SimulationLayer::TurnObserver render = [this, tic_tac_toe](IState& state)
    {
        if (!tic_tac_toe)
        {
            if (state.is_terminal())
            {
                print_console_outcome(state.outcome());
            }
            return;
        }

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
