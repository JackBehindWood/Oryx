#include "GraphicsBoards.h"

#include "Oryx/Board/Graphics/PresentedGraphicsBoard2D.h"
#include "Oryx/Board/Selection.h"
#include "Oryx/Core/Log.h"

namespace oryx
{

namespace selection
{

bool choose_front_end(const std::string& requested_game, bool headless, bool graphics_built, FrontEnd& out_front_end, std::string& out_game)
{
    out_front_end = FrontEnd::Console;
    out_game = requested_game;
    if (headless || !graphics_built)
    {
        return true;
    }

    std::string game;
    if (!choose_game(requested_game, false, game))
    {
        return false;
    }

    out_game = game;
    if (GraphicsBoardRegistry::has(game) || BoardPresenterRegistry::has(game))
    {
        out_front_end = FrontEnd::Graphical;
    }
    else
    {
        OX_INFO("No graphics board or presenter for '{}' - playing in the terminal.", game);
    }
    return true;
}

} // namespace selection

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
