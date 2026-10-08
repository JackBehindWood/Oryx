#pragma once

#include "Oryx/Board/BoardPresentation.h"
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

    // `announce_outcome` prints the result on the terminal the first time the game is seen finished. `seat` is the human's player, or k_all_seats.
    BoardSession(SharedPtr<IBoard> board, bool announce_outcome, PlayerId seat = k_all_seats);

    void on_turn(IState& state) override;
    // The next match exists: the board forgets the finished game and takes the seat chosen by restart.
    void on_match_start() override;
    void on_move() override { ++m_moves; }
    ActionId next_action(const Context& context) override;

    [[nodiscard]] IBoard& board() { return *m_board; }
    [[nodiscard]] bool game_over() const { return m_game_over; }
    // Game actions applied since the match started.
    [[nodiscard]] uint32_t moves() const { return m_moves; }

    void advance(double delta_time);
    // The finished game has been on screen long enough that a restart input should be honoured.
    [[nodiscard]] bool restart_ready() const { return m_game_over && m_seconds_over >= k_restart_delay_seconds; }
    // Ends the finished game for good; `seat` is the human's player in the next one. The board is reset when that match starts.
    void restart(PlayerId seat);
    void restart() { restart(m_next_seat); }

private:
    SharedPtr<IBoard> m_board;
    bool m_announce_outcome;
    PlayerId m_next_seat;
    bool m_restarting = false;
    bool m_game_over = false;
    double m_seconds_over = 0.0;
    uint32_t m_moves = 0;
};

} // namespace oryx
