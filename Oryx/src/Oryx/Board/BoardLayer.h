#pragma once

#include "Oryx/Board/BoardSession.h"
#include "Oryx/Board/Selection.h"
#include "Oryx/Core/Layer.h"

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
// It owns what every board shares: the outcome announcement, restarting a finished game when the board asks, and quitting when stdin runs out.
class BoardLayer : public Layer
{
public:
    explicit BoardLayer(BoardLayerDesc desc);

    void attach() override;
    void update(double delta_time) override;

private:
    void start();

    std::string m_requested_game;
    std::string m_requested_opponent;
    BoardFactory m_create_board;
    bool m_terminal;
    SharedPtr<BoardSession> m_session;
};

} // namespace oryx
