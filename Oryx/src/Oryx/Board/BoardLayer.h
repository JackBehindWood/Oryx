#pragma once

#include "Oryx/Board/BoardSession.h"
#include "Oryx/Board/IGraphicsBoard.h"
#include "Oryx/Board/Selection.h"
#include "Oryx/Core/Layer.h"

namespace oryx
{

struct BoardLayerDesc
{
    // Graphical needs a window and renderer the application has already created.
    selection::FrontEnd front_end = selection::FrontEnd::Console;
    // Empty picks the default (the terminal asks when it can).
    std::string game;
    // A strategy name, kHumanOpponent for hot-seat, or empty to pick the default (the terminal asks when it can).
    std::string opponent;
};

// Plays one game with a human against a strategy (or another human) through the game's registered board; knows no game by name.
// It owns what every board shares: the outcome announcement, restarting a finished windowed game and quitting when stdin runs out.
class BoardLayer : public Layer
{
public:
    explicit BoardLayer(BoardLayerDesc desc);

    void attach() override;
    void update(double delta_time) override;

private:
    void start();

    selection::FrontEnd m_front_end;
    std::string m_requested_game;
    std::string m_requested_opponent;
    SharedPtr<BoardSession> m_session;
};

} // namespace oryx
