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

// The game's registered console board, else a PresentedConsoleBoard over its registered presenter, else the generic ConsoleBoard.
// `seat` is the local human's player, or kAllSeats for hot-seat.
[[nodiscard]] UniquePtr<IConsoleBoard> create_console_board(const std::string& game, PlayerId seat);

} // namespace oryx
