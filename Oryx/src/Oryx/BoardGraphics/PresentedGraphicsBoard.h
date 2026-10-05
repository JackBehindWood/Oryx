#pragma once

#include "Oryx/Board/BoardPresentation.h"
#include "Oryx/Board/IGraphicsBoard.h"
#include "Oryx/BoardGraphics/BoardRenderer2D.h"

namespace oryx
{

// The 2D windowed front end for any game with an IBoardPresenter: clicks become picks, picks become a move, and the board is drawn
// from the scene through the Renderer. A 3D front end would reuse everything here but the layout and draw_board_2d.
class PresentedGraphicsBoard : public IGraphicsBoard
{
public:
    PresentedGraphicsBoard(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat, BoardTheme2D theme = {});

    void on_turn(const IState& state) override;
    ActionId poll_action(const IState& state) override;
    void update(const BoardInput& input, double delta_time) override;
    void render(const BoardInput& input) override;
    bool shows_moves() const override { return true; }

    [[nodiscard]] const BoardPresentation& presentation() const { return m_presentation; }
    [[nodiscard]] SpaceId hovered() const { return m_hovered; }

private:
    void refresh(const IState& state);
    [[nodiscard]] BoardLayout2D layout(const BoardInput& input);

    BoardPresentation m_presentation;
    BoardTheme2D m_theme;
    BoardScene m_scene;
    ActionId m_queued = PENDING_ACTION;
    SpaceId m_hovered = kNoSpace;
};

} // namespace oryx
