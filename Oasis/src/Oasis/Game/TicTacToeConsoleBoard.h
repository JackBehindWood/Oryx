#pragma once

#include "TicTacToeBoardModel.h"

namespace oasis
{

class TicTacToeConsoleBoard : public oryx::IConsoleBoard
{
public:
    void on_turn(const oryx::IState& state) override;
    oryx::ActionId poll_action(const oryx::IState& state) override;
    bool shows_moves() const override { return true; }

private:
    void print() const;
    oryx::ActionId read_move() const;

    TicTacToeBoardModel m_model;
};

} // namespace oasis
