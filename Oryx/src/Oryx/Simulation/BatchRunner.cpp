#include "BatchRunner.h"

#include "Oryx/Simulation/Match.h"

namespace oryx
{

void accumulate(BatchResult& result, const Outcome& outcome)
{
    ++result.matches;

    PlayerId best_player = 0;
    double best_reward = outcome.rewards[0];
    bool tie = false;

    for (size_t player = 1; player < outcome.rewards.player_count(); ++player)
    {
        double reward = outcome.rewards[static_cast<PlayerId>(player)];
        if (reward > best_reward)
        {
            best_reward = reward;
            best_player = static_cast<PlayerId>(player);
            tie = false;
        }
        else if (reward == best_reward)
        {
            tie = true;
        }
    }

    if (tie)
    {
        ++result.draws;
    }
    else
    {
        ++result.wins[static_cast<size_t>(best_player)];
    }

    for (size_t player = 0; player < outcome.rewards.player_count(); ++player)
    {
        result.rewards[static_cast<PlayerId>(player)] += outcome.rewards[static_cast<PlayerId>(player)];
    }
}

BatchRunner::BatchRunner(const IGame& game, SmallVector<IStrategy*, 2> strategies)
    : m_game(game)
    , m_strategies(std::move(strategies))
{
}

BatchResult BatchRunner::run(int32_t match_count)
{
    BatchResult result;
    result.wins.assign(static_cast<size_t>(m_game.num_players()), 0);
    result.rewards = Rewards<double>(static_cast<size_t>(m_game.num_players()));

    for (int32_t i = 0; i < match_count; ++i)
    {
        Match match(m_game, m_strategies);
        accumulate(result, match.play());
    }

    return result;
}

} // namespace oryx
