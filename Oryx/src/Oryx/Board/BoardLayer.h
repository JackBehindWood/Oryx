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
    // A strategy name, k_human_opponent for hot-seat, or empty to pick the default (the terminal asks when it can).
    std::string opponent;
    // Graphical only: makes the windowed board. Empty uses GraphicsBoardRegistry alone; applications pass BoardGraphics' create_graphics_board,
    // which also serves games that register only a presenter.
    GraphicsBoardFactory create_graphics_board;
};

// Plays one game with a human against a strategy (or another human) through the game's registered board; knows no game by name.
// It owns what every board shares: the outcome announcement, reading a window's input once per frame (BoardInput), restarting a finished
// windowed game and quitting when stdin runs out.
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
    GraphicsBoardFactory m_create_graphics_board;
    SharedPtr<BoardSession> m_session;
    IGraphicsBoard* m_graphics_board = nullptr;
};

} // namespace oryx
