#pragma once

#include "Oasis/Game/TicTacToeBoardModel.h"
#include "TicTacToeLayout.h"

namespace oasis
{

// Draws a TicTacToeBoardModel through Renderer::draw_*, and turns clicks on its cells into ActionIds.
class TicTacToeGraphicsBoard : public oryx::IGraphicsBoard
{
public:
    void on_turn(const oryx::IState& state) override;
    oryx::ActionId poll_action(const oryx::IState& state) override;
    void render(double delta_time) override;
    bool shows_moves() const override { return true; }

private:
    [[nodiscard]] TicTacToeLayout layout() const;
    [[nodiscard]] oryx::Vec2f cursor_world(const TicTacToeLayout& layout) const;
    void draw_status(const TicTacToeLayout& layout) const;

    TicTacToeBoardModel m_model;
};

} // namespace oasis
