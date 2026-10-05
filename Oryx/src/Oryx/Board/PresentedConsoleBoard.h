#pragma once

#include "Oryx/Board/BoardPresentation.h"
#include "Oryx/Board/IConsoleBoard.h"

namespace oryx
{

// The terminal front end for any game with an IBoardPresenter: prints the board after every change and reads moves as picks typed by label
// ("a2 a3", one pick per word, over one or several lines).
class PresentedConsoleBoard : public IConsoleBoard
{
public:
    PresentedConsoleBoard(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat);

    void on_turn(const IState& state) override;
    ActionId poll_action(const IState& state) override;
    bool shows_moves() const override { return true; }

private:
    void print() const;

    BoardPresentation m_presentation;
};

// The board as text: one row per distinct y, one column per distinct x, axis labels when the view has them,
// the last move's spaces in [brackets] and picked spaces in (parentheses).
[[nodiscard]] std::string board_text(const BoardScene& scene);

} // namespace oryx
