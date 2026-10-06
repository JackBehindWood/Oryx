#pragma once

#include "Oryx/Board/IBoard.h"
#include "Oryx/Simulation/ITurnObserver.h"
#include "Oryx/Strategy/IActionSource.h"

namespace oryx
{

// One game played through a board: feeds the simulation's turns to the board, serves the human seats from it and tracks when the game is over.
// Shared with the simulation, which keeps it alive independently of the layer that created it.
class BoardSession : public ITurnObserver, public IActionSource
{
public:
    static constexpr double k_restart_delay_seconds = 0.4;

    // `announce_outcome` prints the result on the terminal the first time the game is seen finished.
    BoardSession(SharedPtr<IBoard> board, bool announce_outcome);

    void on_turn(IState& state) override;
    ActionId next_action(const Context& context) override;

    [[nodiscard]] IBoard& board() { return *m_board; }
    [[nodiscard]] bool game_over() const { return m_game_over; }

    void advance(double delta_time);
    // The finished game has been on screen long enough that a restart input should be honoured.
    [[nodiscard]] bool restart_ready() const { return m_game_over && m_seconds_over >= k_restart_delay_seconds; }
    void restart();

private:
    SharedPtr<IBoard> m_board;
    bool m_announce_outcome;
    bool m_game_over = false;
    double m_seconds_over = 0.0;
};

} // namespace oryx
