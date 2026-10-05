#include "BoardLayer.h"

#include "Oryx/Board/ConsoleBoard.h"
#include "Oryx/Core/Application.h"
#include "Oryx/Core/Log.h"
#include "Oryx/Core/Input.h"
#include "Oryx/Core/KeyCode.h"
#include "Oryx/Core/Random.h"
#include "Oryx/Events/SimulationEvent.h"
#include "Oryx/Strategy/ExternalStrategy.h"

namespace oryx
{

namespace
{

bool restart_requested()
{
    return Input::key_pressed(KeyCode::R) || Input::mouse_pressed(MouseCode::Left);
}

} // namespace

BoardLayer::BoardLayer(BoardLayerDesc desc)
    : Layer("BoardLayer")
    , m_front_end(desc.front_end)
    , m_requested_game(std::move(desc.game))
    , m_requested_opponent(std::move(desc.opponent))
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

    if (Input::key_pressed(KeyCode::Escape))
    {
        Application::Get().close();
        return;
    }

    m_session->advance(delta_time);
    if (m_session->restart_ready() && restart_requested())
    {
        m_session->restart();
        RestartSimulationEvent restart;
        Application::Get().post_event(restart);
    }
    static_cast<IGraphicsBoard&>(m_session->board()).render(delta_time);
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

    SharedPtr<IBoard> board;
    if (graphical)
    {
        UniquePtr<IGraphicsBoard> graphics_board = GraphicsBoardRegistry::create(game_name);
        if (graphics_board == nullptr)
        {
            throw Error("No graphics board is registered for '" + game_name + "'");
        }
        board = SharedPtr<IBoard>(std::move(graphics_board));
    }
    else
    {
        board = SharedPtr<IBoard>(create_console_board(game_name));
    }
    m_session = create_shared<BoardSession>(board, !graphical);

    size_t seat_count = static_cast<size_t>(game->num_players());
    SmallVector<UniquePtr<IStrategy>, 2> strategies(seat_count);

    if (opponent_name == selection::kHumanOpponent)
    {
        for (size_t seat = 0; seat < seat_count; ++seat)
        {
            strategies[seat] = create_unique<ExternalStrategy>(m_session);
        }
        OX_INFO("Human vs human.");
    }
    else
    {
        Random random;
        size_t human_seat = static_cast<size_t>(random.get_int(0, static_cast<int64_t>(seat_count) - 1));
        for (size_t seat = 0; seat < seat_count; ++seat)
        {
            strategies[seat] = seat == human_seat ? create_unique<ExternalStrategy>(m_session) : selection::create_opponent(opponent_name, !board->shows_moves());
        }
        OX_INFO("You are player {} against '{}'.", human_seat + 1, opponent_name);
    }

    StartSimulationEvent event(std::move(game), std::move(strategies), 1, m_session, false, graphical);
    Application::Get().post_event(event);
}

} // namespace oryx
