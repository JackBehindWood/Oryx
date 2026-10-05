#pragma once

#include "TicTacToeGame.h"

namespace oasis
{

// What a TicTacToe state means to a person, shared by the console and graphics boards: they only differ in how they show it and read moves.
class TicTacToeBoardModel
{
public:
    static constexpr size_t kSize = 3;

    void update(const oryx::IState& state);

    [[nodiscard]] Mark mark_at(size_t row, size_t col) const { return m_marks[row][col]; }
    [[nodiscard]] oryx::PlayerId current_player() const { return m_current_player; }
    [[nodiscard]] bool terminal() const { return m_terminal; }
    // The winning player (0 plays X), or -1 for a draw or an unfinished game.
    [[nodiscard]] int32_t winner() const { return m_winner; }
    [[nodiscard]] bool legal(size_t row, size_t col) const;
    [[nodiscard]] std::string status_text() const;

    [[nodiscard]] static oryx::ActionId action_for(size_t row, size_t col) { return static_cast<oryx::ActionId>(row * kSize + col); }
    [[nodiscard]] static Mark mark_of(oryx::PlayerId player) { return player == 0 ? Mark::X : Mark::O; }
    [[nodiscard]] static char symbol_of(Mark mark);

private:
    Mark m_marks[kSize][kSize] = {};
    oryx::ActionList m_legal;
    oryx::PlayerId m_current_player = 0;
    bool m_terminal = false;
    int32_t m_winner = -1;
};

} // namespace oasis
