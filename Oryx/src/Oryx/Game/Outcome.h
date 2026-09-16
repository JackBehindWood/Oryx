#pragma once

namespace oryx
{

template<typename T>
class Rewards
{
public:
    explicit Rewards(size_t player_count) : m_values(player_count, T{}) {}

    inline size_t player_count() const { return m_values.size(); }

    T& operator[](size_t player) { return m_values[player]; }
    const T& operator[](size_t player) const { return m_values[player]; }

    inline T& at(size_t player) { return m_values.at(player); }
    inline const T& at(size_t player) const { return m_values.at(player); }

private:
    std::vector<T> m_values;
};

struct Outcome
{
    bool is_terminal = false;
    Rewards<double> rewards{ 0 };
};

} // namespace oryx
