#pragma once

#include "Oryx/Board/IConsoleBoard.h"

namespace oryx
{

// Text UI for games without a dedicated board: legal moves are listed with their action_to_string() and chosen by ActionId.
class ConsoleBoard : public IConsoleBoard
{
public:
    void on_turn(const IState& state) override;
    ActionId poll_action(const IState& state) override;
    bool shows_moves() const override { return false; }
};

// The game's registered console board, else the generic ConsoleBoard.
[[nodiscard]] UniquePtr<IConsoleBoard> create_console_board(const std::string& game);

} // namespace oryx
