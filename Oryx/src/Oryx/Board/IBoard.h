#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/IState.h"

namespace oryx
{

// How one game is shown to and played by a human; a front-end layer drives it and knows nothing about the game behind it.
class IBoard
{
public:
    virtual ~IBoard() = default;

    // Called every simulation update before the decision, including on the finished state.
    virtual void on_turn(const IState& state) = 0;
    // The human's move for the current seat, UNDO_ACTION to take one back, or PENDING_ACTION while none is chosen yet.
    virtual ActionId poll_action(const IState& state) = 0;
    // True when the board itself shows what every seat played, so opponents need not announce their moves.
    [[nodiscard]] virtual bool shows_moves() const = 0;
    // A new game is about to be shown, with `seat` (a player or k_all_seats) now the human's: drop everything left over from the last one.
    virtual void reset(PlayerId) {}
    // True once if the person asked to play again since the last call; BoardLayer restarts a finished game when it is. Terminal boards never ask.
    [[nodiscard]] virtual bool take_restart_request() { return false; }
};

// Makes the board for a game and seat (a player, or k_all_seats for hot-seat); applications supply it for a board that is not the terminal one.
using BoardFactory = std::function<SharedPtr<IBoard>(const std::string& game, PlayerId seat)>;
// Undoes what the factory registered for a board (a window client, say); called before the board is dropped, so nothing keeps a reference to it.
using BoardReleaser = std::function<void(IBoard&)>;

} // namespace oryx
