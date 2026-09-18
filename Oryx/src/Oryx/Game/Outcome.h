#pragma once

#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Game/PlayerId.h"

namespace oryx
{

// Sized inline for 2 players (DESIGN.md §19); spills to heap past that.
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

} // namespace oryx
