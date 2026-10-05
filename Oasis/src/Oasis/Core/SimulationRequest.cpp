#include "SimulationRequest.h"

#include "Oryx/Board/Selection.h"

namespace oasis
{

using namespace oryx::selection;

oryx::StartSimulationEvent make_simulation_request(const Options& options)
{
    std::string game_name;
    if (!choose_game(options.game, false, game_name))
    {
        throw oryx::Error("No game to simulate");
    }
    oryx::UniquePtr<oryx::IGame> game = oryx::create_game(game_name);
    OX_INFO("Playing {}.", game->name());

    size_t first_comma = options.simulate.find(',');
    size_t second_comma = first_comma == std::string::npos ? std::string::npos : options.simulate.find(',', first_comma + 1);
    if (first_comma == std::string::npos || second_comma == std::string::npos)
    {
        throw oryx::Error("--simulate expects <strategyA>,<strategyB>,<matchCount>, got '" + options.simulate + "'");
    }

    std::string strategy_a_name = options.simulate.substr(0, first_comma);
    std::string strategy_b_name = options.simulate.substr(first_comma + 1, second_comma - first_comma - 1);
    std::string count_string = options.simulate.substr(second_comma + 1);

    require_creatable("strategy", strategy_a_name, oryx::StrategyRegistry::info(strategy_a_name));
    require_creatable("strategy", strategy_b_name, oryx::StrategyRegistry::info(strategy_b_name));

    std::vector<std::string> available = strategies_for(game_name);
    bool known = std::find(available.begin(), available.end(), strategy_a_name) != available.end() &&
                 std::find(available.begin(), available.end(), strategy_b_name) != available.end();
    if (!known)
    {
        throw oryx::Error("--simulate: '" + strategy_a_name + "' and '" + strategy_b_name + "' must both be strategies for '" + game_name + "' (" + joined(available) + ")");
    }

    int32_t match_count = 0;
    try
    {
        match_count = std::stoi(count_string);
    }
    catch (const std::exception&)
    {
        throw oryx::Error("--simulate: invalid match count '" + count_string + "'");
    }

    if (game->num_players() != 2)
    {
        throw oryx::Error("--simulate needs a two-player game, but '" + game_name + "' has " + std::to_string(game->num_players()) + " players");
    }

    oryx::SmallVector<oryx::UniquePtr<oryx::IStrategy>, 2> strategies;
    strategies.push_back(oryx::StrategyRegistry::create(strategy_a_name));
    strategies.push_back(oryx::StrategyRegistry::create(strategy_b_name));

    return oryx::StartSimulationEvent(std::move(game), std::move(strategies), match_count, nullptr, options.benchmark);
}

} // namespace oasis
