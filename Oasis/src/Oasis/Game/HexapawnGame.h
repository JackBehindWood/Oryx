#pragma once

#include "Oryx.h"

namespace oasis
{

// Martin Gardner's Hexapawn: three pawns each on a 3x3 board. A pawn steps forward onto an empty square or captures diagonally forward.
// A player wins by reaching the far row, or when the opponent cannot move (which includes having no pawns left). There are no draws.
// Squares are numbered row * 3 + col with row 0 at the top; player 0 starts on the bottom row and moves up.
class HexapawnState : public oryx::IState
{
public:
    static constexpr uint32_t kSize = 3;
    static constexpr int8_t kEmpty = -1;

    // How a pawn moves, as seen from the board: towards column - 1, straight, or towards column + 1.
    enum Direction : uint32_t
    {
        Left = 0,
        Forward = 1,
        Right = 2
    };

    HexapawnState();

    oryx::ActionList legal_actions() const override;

    void apply(oryx::ActionId action) override;
    void undo(oryx::ActionId action) override;

    oryx::PlayerId current_player() const override { return m_current_player; }

    bool is_terminal() const override;
    oryx::Outcome outcome() const override;

    std::string action_to_string(oryx::ActionId action) const override;

    // The owner of the pawn on a square, or kEmpty.
    [[nodiscard]] int8_t owner_at(uint32_t square) const { return m_squares[square]; }

    [[nodiscard]] static oryx::ActionId action_for(uint32_t from, Direction direction) { return from * kSize + direction; }
    [[nodiscard]] static uint32_t from_square(oryx::ActionId action) { return action / kSize; }
    // The square `action` moves to when played by `player`.
    [[nodiscard]] static uint32_t to_square(oryx::ActionId action, oryx::PlayerId player);
    [[nodiscard]] static std::string square_name(uint32_t square);

private:
    // The player who has won, or -1 while the game is running.
    [[nodiscard]] oryx::PlayerId winner() const;
    [[nodiscard]] bool reached_far_row(oryx::PlayerId player) const;
    void generate(oryx::ActionList& out) const;

    std::array<int8_t, kSize * kSize> m_squares;
    oryx::PlayerId m_current_player = 0;
};

class HexapawnGame : public oryx::IGame
{
public:
    oryx::UniquePtr<oryx::IState> new_initial_state() const override
    {
        return oryx::create_unique<HexapawnState>();
    }

    std::string name() const override { return "Hexapawn"; }
    int32_t num_players() const override { return 2; }
};

} // namespace oasis
