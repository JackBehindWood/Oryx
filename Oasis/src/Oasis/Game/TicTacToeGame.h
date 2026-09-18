#pragma once

#include "Oryx.h"

namespace oasis
{

enum class Mark
{
    Empty,
    X,
    O
};

class TicTacToeState : public oryx::IState
{
public:
    TicTacToeState() = default;

    oryx::ActionList legal_actions() const override;

    void apply(oryx::ActionId action) override;
    void undo(oryx::ActionId action) override;

    oryx::PlayerId current_player() const override { return m_current_player; }

    bool is_terminal() const override;
    oryx::Outcome outcome() const override;

    std::string action_to_string(oryx::ActionId action) const override;

    Mark mark_at(size_t row, size_t col) const { return m_board.at(row, col); }

private:
    Mark winner() const;

    oryx::Matrix<3, 3, Mark> m_board;
    oryx::PlayerId m_current_player = 0;
};

// Decodes a TicTacToe ActionId into {row, col} - the worked example for
// IActionFeatures (ARCHITECTURE.md §14).
class TicTacToeActionFeatures : public oryx::IActionFeatures
{
public:
    oryx::SmallVector<int32_t, 2> decode(oryx::ActionId action) const override
    {
        return { static_cast<int32_t>(action / 3), static_cast<int32_t>(action % 3) };
    }
};

class TicTacToeGame : public oryx::IGame
{
public:
    oryx::UniquePtr<oryx::IState> new_initial_state() const override
    {
        return oryx::create_unique<TicTacToeState>();
    }

    std::string name() const override { return "TicTacToe"; }
    int32_t num_players() const override { return 2; }

    oryx::IActionFeatures* action_features() const override
    {
        static TicTacToeActionFeatures instance;
        return &instance;
    }
};

} // namespace oasis
