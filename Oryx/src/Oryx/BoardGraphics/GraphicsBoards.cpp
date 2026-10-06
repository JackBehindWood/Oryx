#include "GraphicsBoards.h"

#include "Oryx/BoardGraphics/PresentedGraphicsBoard2D.h"

namespace oryx
{

UniquePtr<IGraphicsBoard> create_graphics_board(const std::string& game, PlayerId seat)
{
    if (UniquePtr<IGraphicsBoard> board = GraphicsBoardRegistry::create(game))
    {
        return board;
    }
    if (UniquePtr<IBoardPresenter> presenter = BoardPresenterRegistry::create(game))
    {
        return create_unique<PresentedGraphicsBoard2D>(std::move(presenter), game, seat);
    }
    return nullptr;
}

} // namespace oryx
