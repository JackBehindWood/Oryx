#include "BoardLayer.h"

#include "Oryx/Board/BoardPresentation.h"

#include "Oryx/Board/Console/ConsoleBoard.h"
#include "Oryx/Core/Application.h"
#include "Oryx/Core/Log.h"
#include "Oryx/Core/Random.h"
#include "Oryx/Events/SimulationEvent.h"
#include "Oryx/Strategy/ExternalStrategy.h"

namespace oryx
{

BoardLayer::BoardLayer(BoardLayerDesc desc)
    : Layer("BoardLayer")
    , m_requested_game(std::move(desc.game))
    , m_requested_opponent(std::move(desc.opponent))
    , m_create_board(std::move(desc.create_board))
    , m_release_board(std::move(desc.release_board))
    , m_decision_observer(desc.decision_observer)
    , m_terminal(desc.terminal)
{
}

void BoardLayer::attach()
{
    try
    {
        std::string game_name;
        std::string opponent_name;
        if (!selection::choose_game(m_requested_game, m_terminal, game_name) || !selection::choose_opponent(game_name, m_requested_opponent, m_terminal, opponent_name))
        {
            Application::Get().close(1);
            return;
        }
        start(game_name, opponent_name);
    }
    catch (const Error& error)
    {
        error.log();
        Application::Get().close(1);
    }
}

void BoardLayer::detach()
{
    release_current_board();
}

void BoardLayer::event(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<StartMatchEvent>(OX_BIND_EVENT_FN(on_start_match));
}

bool BoardLayer::on_start_match(StartMatchEvent& event)
{
    m_pending_game = event.game();
    m_pending_opponent = event.opponent();
    m_switch_requested = true;
    return true;
}

void BoardLayer::release_current_board()
{
    if (m_session && m_release_board)
    {
        m_release_board(m_session->board());
    }
}

void BoardLayer::apply_pending_match()
{
    m_switch_requested = false;
    try
    {
        std::string game_name = m_pending_game.empty() ? m_game_name : m_pending_game;
        std::string requested_opponent = m_pending_opponent.empty() ? m_opponent_name : m_pending_opponent;
        std::vector<std::string> fitting = selection::opponents_for(game_name);
        if (m_pending_opponent.empty() && std::find(fitting.begin(), fitting.end(), requested_opponent) == fitting.end())
        {
            requested_opponent.clear();
        }

        std::string checked_game;
        std::string opponent_name;
        if (!selection::choose_game(game_name, false, checked_game) || !selection::choose_opponent(checked_game, requested_opponent, false, opponent_name))
        {
            return;
        }
        start(checked_game, opponent_name);
    }
    catch (const Error& error)
    {
        error.log();
    }
}

void BoardLayer::update(double delta_time)
{
    if (!m_session)
    {
        return;
    }

    if (m_terminal)
    {
        // failbit, not eofbit: an unterminated last line sets eofbit but still yields a move.
        if (std::cin.fail())
        {
            Application::Get().close();
        }
        return;
    }

    if (m_switch_requested)
    {
        apply_pending_match();
    }

    m_session->advance(delta_time);
    bool restart_requested = m_session->board().take_restart_request();
    if (restart_requested && m_session->restart_ready())
    {
        SmallVector<uint32_t, 2> order = draw_next_seats();
        m_session->restart(m_human_seat);
        RestartSimulationEvent restart(std::move(order));
        Application::Get().post_event(restart);
    }
}

SmallVector<uint32_t, 2> BoardLayer::draw_next_seats()
{
    if (m_human_seat == k_all_seats)
    {
        return {};
    }
    size_t current = 0;
    while (current < m_seat_order.size() && m_seat_order[current] != m_human_slot)
    {
        ++current;
    }
    PlayerId next = static_cast<PlayerId>(m_random.get_int(0, static_cast<int64_t>(m_seat_order.size()) - 1));
    std::swap(m_seat_order[current], m_seat_order[static_cast<size_t>(next)]);
    m_human_seat = next;
    OX_INFO("Next game: you are player {}.", next + 1);
    return m_seat_order;
}

void BoardLayer::start(const std::string& game_name, const std::string& opponent_name)
{
    UniquePtr<IGame> game = create_game(game_name);
    OX_INFO("Playing {}.", game->name());

    size_t seat_count = static_cast<size_t>(game->num_players());
    bool hot_seat = opponent_name == selection::k_human_opponent;
    PlayerId human_seat = k_all_seats;
    if (!hot_seat)
    {
        human_seat = static_cast<PlayerId>(m_random.get_int(0, static_cast<int64_t>(seat_count) - 1));
    }

    SharedPtr<IBoard> board = m_create_board ? m_create_board(game_name, human_seat) : SharedPtr<IBoard>(create_console_board(game_name, human_seat));
    if (board == nullptr)
    {
        throw Error("No board is available for '" + game_name + "'");
    }
    release_current_board();
    m_human_seat = human_seat;
    m_human_slot = static_cast<uint32_t>(human_seat < 0 ? 0 : human_seat);
    m_seat_order.clear();
    for (uint32_t seat = 0; seat < seat_count; ++seat)
    {
        m_seat_order.push_back(seat);
    }
    m_game_name = game_name;
    m_opponent_name = opponent_name;
    m_session = create_shared<BoardSession>(board, m_terminal, human_seat);

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

    StartSimulationEvent event(std::move(game), std::move(strategies), 1, m_session, false, !m_terminal, m_decision_observer);
    Application::Get().post_event(event);
}

} // namespace oryx
