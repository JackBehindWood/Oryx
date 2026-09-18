#pragma once

#include "Oryx/Game/PlayerId.h"

namespace oryx
{

template<typename T>
class Rewards
{
public:
    explicit Rewards(size_t player_count) : m_values(player_count, T{}) {}

    inline size_t player_count() const { return m_values.size(); }

    T& operator[](PlayerId player) { return m_values[static_cast<size_t>(player)]; }
    const T& operator[](PlayerId player) const { return m_values[static_cast<size_t>(player)]; }

    inline T& at(PlayerId player) { return m_values.at(static_cast<size_t>(player)); }
    inline const T& at(PlayerId player) const { return m_values.at(static_cast<size_t>(player)); }

private:
    std::vector<T> m_values;
};

struct Outcome
{
    bool is_terminal = false;
    Rewards<double> rewards{ 0 };
};

} // namespace oryx
