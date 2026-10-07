#pragma once

#include "Oryx/Board/BoardSession.h"
#include "Oryx/Board/Selection.h"
#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Core/Layer.h"
#include "Oryx/Core/Random.h"

namespace oryx
{

struct BoardLayerDesc
{
    // Empty picks the default (the terminal asks when it can).
    std::string game;
    // A strategy name, k_human_opponent for hot-seat, or empty to pick the default (the terminal asks when it can).
    std::string opponent;
    // Makes the board; empty plays in the terminal. A windowed board comes from here, with whatever feeds it frames registered by the factory itself.
    BoardFactory create_board;
    // The board reads stdin: choices are prompted for, the outcome is printed, and the application closes when stdin runs out.
    bool terminal = true;
};

// Plays one game with a human against a strategy (or another human) through the game's board; knows no game by name, and no window, device or renderer.
// It owns what every board shares: the outcome announcement, restarting a finished game when the board asks (against a strategy, the human's seat, and so who moves first, is drawn again for each game), and quitting when stdin runs out.
class BoardLayer : public Layer
{
public:
    explicit BoardLayer(BoardLayerDesc desc);

    void attach() override;
    void update(double delta_time) override;

private:
    void start();
    // Draws the human's seat for the next game and returns the strategy for each seat that goes with it.
    [[nodiscard]] SmallVector<uint32_t, 2> draw_next_seats();

    std::string m_requested_game;
    std::string m_requested_opponent;
    BoardFactory m_create_board;
    bool m_terminal;
    SharedPtr<BoardSession> m_session;
    Random m_random;
    // The human's player, or k_all_seats in hot-seat; `m_seat_order[seat]` is the index among the started strategies that plays `seat`, and the human's is `m_human_slot`.
    PlayerId m_human_seat = k_all_seats;
    uint32_t m_human_slot = 0;
    SmallVector<uint32_t, 2> m_seat_order;
};

} // namespace oryx
