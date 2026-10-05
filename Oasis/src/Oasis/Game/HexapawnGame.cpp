#include "HexapawnGame.h"

namespace oasis
{

namespace
{

constexpr uint32_t kSize = HexapawnState::kSize;

int32_t forward(oryx::PlayerId player)
{
    return player == 0 ? -1 : 1;
}

uint32_t home_row(oryx::PlayerId player)
{
    return player == 0 ? kSize - 1 : 0;
}

} // namespace

HexapawnState::HexapawnState()
{
    m_squares.fill(kEmpty);
    for (uint32_t col = 0; col < kSize; ++col)
    {
        m_squares[home_row(0) * kSize + col] = 0;
        m_squares[home_row(1) * kSize + col] = 1;
    }
}

uint32_t HexapawnState::to_square(oryx::ActionId action, oryx::PlayerId player)
{
    uint32_t from = from_square(action);
    int32_t row = static_cast<int32_t>(from / kSize) + forward(player);
    int32_t col = static_cast<int32_t>(from % kSize) + static_cast<int32_t>(action % kSize) - 1;
    return static_cast<uint32_t>(row) * kSize + static_cast<uint32_t>(col);
}

std::string HexapawnState::square_name(uint32_t square)
{
    return std::string(1, static_cast<char>('a' + square % kSize)) + std::to_string(kSize - square / kSize);
}

void HexapawnState::generate(oryx::ActionList& out) const
{
    for (uint32_t from = 0; from < kSize * kSize; ++from)
    {
        if (m_squares[from] != m_current_player)
        {
            continue;
        }
        int32_t row = static_cast<int32_t>(from / kSize) + forward(m_current_player);
        if (row < 0 || row >= static_cast<int32_t>(kSize))
        {
            continue;
        }
        for (uint32_t direction = Left; direction <= Right; ++direction)
        {
            int32_t col = static_cast<int32_t>(from % kSize) + static_cast<int32_t>(direction) - 1;
            if (col < 0 || col >= static_cast<int32_t>(kSize))
            {
                continue;
            }
            int8_t target = m_squares[static_cast<uint32_t>(row) * kSize + static_cast<uint32_t>(col)];
            bool legal = direction == Forward ? target == kEmpty : target == 1 - m_current_player;
            if (legal)
            {
                out.push_back(action_for(from, static_cast<Direction>(direction)));
            }
        }
    }
}

oryx::ActionList HexapawnState::legal_actions() const
{
    oryx::ActionList actions;
    if (!reached_far_row(0) && !reached_far_row(1))
    {
        generate(actions);
    }
    return actions;
}

void HexapawnState::apply(oryx::ActionId action)
{
    m_squares[to_square(action, m_current_player)] = static_cast<int8_t>(m_current_player);
    m_squares[from_square(action)] = kEmpty;
    m_current_player = 1 - m_current_player;
}

void HexapawnState::undo(oryx::ActionId action)
{
    m_current_player = 1 - m_current_player;
    uint32_t to = to_square(action, m_current_player);
    m_squares[from_square(action)] = static_cast<int8_t>(m_current_player);
    m_squares[to] = action % kSize == Forward ? kEmpty : static_cast<int8_t>(1 - m_current_player);
}

bool HexapawnState::reached_far_row(oryx::PlayerId player) const
{
    uint32_t far_row = home_row(1 - player);
    for (uint32_t col = 0; col < kSize; ++col)
    {
        if (m_squares[far_row * kSize + col] == player)
        {
            return true;
        }
    }
    return false;
}

oryx::PlayerId HexapawnState::winner() const
{
    for (oryx::PlayerId player : { 0, 1 })
    {
        if (reached_far_row(player))
        {
            return player;
        }
    }
    oryx::ActionList moves;
    generate(moves);
    return moves.empty() ? 1 - m_current_player : -1;
}

bool HexapawnState::is_terminal() const
{
    return winner() >= 0;
}

oryx::Outcome HexapawnState::outcome() const
{
    oryx::Outcome result;
    result.rewards = oryx::Rewards<double>(2);

    oryx::PlayerId win = winner();
    result.is_terminal = win >= 0;
    if (win >= 0)
    {
        result.rewards[win] = 1.0;
        result.rewards[1 - win] = -1.0;
    }
    return result;
}

std::string HexapawnState::action_to_string(oryx::ActionId action) const
{
    const char* separator = action % kSize == Forward ? "-" : "x";
    return square_name(from_square(action)) + separator + square_name(to_square(action, m_current_player));
}

} // namespace oasis

OX_REGISTER_GAME(oasis::HexapawnGame, "hexapawn", {}, "Gardner's three-pawn game on a 3x3 board")
