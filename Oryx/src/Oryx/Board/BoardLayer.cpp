#include "BoardLayer.h"

#include "Oryx/Board/BoardPresentation.h"
#include "Oryx/Board/ConsoleBoard.h"
#include "Oryx/Core/Application.h"
#include "Oryx/Core/Log.h"
#include "Oryx/Core/Random.h"
#include "Oryx/Core/Window.h"
#include "Oryx/Events/SimulationEvent.h"
#include "Oryx/Strategy/ExternalStrategy.h"

namespace oryx
{

namespace
{

BoardInput capture_input()
{
    Window* window = Application::Get().window();
    if (window == nullptr)
    {
        return {};
    }
    NativeWindowHandle handle = window->native_handle();
    return read_board_input(window->input(), { static_cast<float>(handle.width), static_cast<float>(handle.height) });
}

UniquePtr<IGraphicsBoard> registered_graphics_board(const std::string& game, PlayerId)
{
    return GraphicsBoardRegistry::create(game);
}

} // namespace

BoardLayer::BoardLayer(BoardLayerDesc desc)
    : Layer("BoardLayer")
    , m_front_end(desc.front_end)
    , m_requested_game(std::move(desc.game))
    , m_requested_opponent(std::move(desc.opponent))
    , m_create_graphics_board(desc.create_graphics_board ? std::move(desc.create_graphics_board) : GraphicsBoardFactory(registered_graphics_board))
{
}

void BoardLayer::attach()
{
    try
    {
        start();
    }
    catch (const Error& error)
    {
        error.log();
        Application::Get().close(1);
    }
}

void BoardLayer::update(double delta_time)
{
    if (!m_session)
    {
        return;
    }

    if (m_front_end == selection::FrontEnd::Console)
    {
        // failbit, not eofbit: an unterminated last line sets eofbit but still yields a move.
        if (std::cin.fail())
        {
            Application::Get().close();
        }
        return;
    }

    BoardInput input = capture_input();
    if (input.quit)
    {
        Application::Get().close();
        return;
    }

    m_session->advance(delta_time);
    if (m_session->restart_ready() && input.restart)
    {
        m_session->restart();
        RestartSimulationEvent restart;
        Application::Get().post_event(restart);
    }
    m_graphics_board->update(input, delta_time);
    m_graphics_board->render(input);
}

void BoardLayer::start()
{
    bool graphical = m_front_end == selection::FrontEnd::Graphical;

    std::string game_name;
    std::string opponent_name;
    if (!selection::choose_game(m_requested_game, !graphical, game_name) || !selection::choose_opponent(game_name, m_requested_opponent, !graphical, opponent_name))
    {
        Application::Get().close(1);
        return;
    }

    UniquePtr<IGame> game = create_game(game_name);
    OX_INFO("Playing {}.", game->name());

    size_t seat_count = static_cast<size_t>(game->num_players());
    bool hot_seat = opponent_name == selection::kHumanOpponent;
    PlayerId human_seat = kAllSeats;
    if (!hot_seat)
    {
        Random random;
        human_seat = static_cast<PlayerId>(random.get_int(0, static_cast<int64_t>(seat_count) - 1));
    }

    SharedPtr<IBoard> board;
    if (graphical)
    {
        UniquePtr<IGraphicsBoard> graphics_board = m_create_graphics_board(game_name, human_seat);
        if (graphics_board == nullptr)
        {
            throw Error("No graphics board is registered for '" + game_name + "'");
        }
        m_graphics_board = graphics_board.get();
        board = SharedPtr<IBoard>(std::move(graphics_board));
    }
    else
    {
        board = SharedPtr<IBoard>(create_console_board(game_name, human_seat));
    }
    m_session = create_shared<BoardSession>(board, !graphical);

    SmallVector<UniquePtr<IStrategy>, 2> strategies(seat_count);
    for (size_t seat = 0; seat < seat_count; ++seat)
    {
        bool human = hot_seat || static_cast<PlayerId>(seat) == human_seat;
        strategies[seat] = human ? create_unique<ExternalStrategy>(m_session) : selection::create_opponent(opponent_name, !board->shows_moves());
    }
    if (hot_seat)
    {
        OX_INFO("Human vs human.");
    }
    else
    {
        OX_INFO("You are player {} against '{}'.", human_seat + 1, opponent_name);
    }

    StartSimulationEvent event(std::move(game), std::move(strategies), 1, m_session, false, graphical);
    Application::Get().post_event(event);
}

} // namespace oryx
