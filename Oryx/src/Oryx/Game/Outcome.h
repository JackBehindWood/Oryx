#pragma once

#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Game/PlayerId.h"

namespace oryx
{

// Sized inline for 2 players (docs/design/decision-log.md); spills to heap past that.
template<typename T>
class Rewards
{
public:
    explicit Rewards(size_t player_count) : m_values(player_count, T{}) {}

    inline size_t player_count() const { return m_values.size(); }

    T& operator[](PlayerId player) { return m_values[static_cast<size_t>(player)]; }
    const T& operator[](PlayerId player) const { return m_values[static_cast<size_t>(player)]; }

    inline T& at(PlayerId player) { return m_values[static_cast<size_t>(player)]; }
    inline const T& at(PlayerId player) const { return m_values[static_cast<size_t>(player)]; }

private:
    SmallVector<T, 2> m_values;
};

struct Outcome
{
    bool is_terminal = false;
    Rewards<double> rewards{ 0 };
};

// The player with the strictly highest reward, or -1 for a draw (or no players).
[[nodiscard]] inline int32_t winner_of(const Outcome& outcome)
{
    size_t best = 0;
    bool tie = false;
    for (size_t player = 1; player < outcome.rewards.player_count(); ++player)
    {
        double reward = outcome.rewards[static_cast<PlayerId>(player)];
        double best_reward = outcome.rewards[static_cast<PlayerId>(best)];
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
    return tie || outcome.rewards.player_count() == 0 ? -1 : static_cast<int32_t>(best);
}

} // namespace oryx
