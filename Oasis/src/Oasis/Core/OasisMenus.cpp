#include "OasisMenus.h"

#ifdef OX_ENABLE_GRAPHICS

namespace oasis
{

namespace
{

using namespace oryx;

constexpr const char* k_confirm = "Switch match?";

void dispatch_switch(SwitchState& state, DashboardFeed& feed)
{
    feed.clear_at_next_match();
    StartMatchEvent request(state.game, state.opponent);
    Application::Get().post_event(request);
}

void request_switch(SwitchState& state, const BoardLayer* board, DashboardFeed& feed, const std::string& game, const std::string& opponent)
{
    if (board == nullptr || ((game.empty() || game == board->game_name()) && (opponent.empty() || opponent == board->opponent_name())))
    {
        return;
    }
    state.game = game;
    state.opponent = opponent;
    if (!board->match_in_progress())
    {
        dispatch_switch(state, feed);
        return;
    }
    state.pending = true;
    state.confirm_requested = true;
}

} // namespace

void game_menu(SwitchState& state, const BoardLayer* board, DashboardFeed& feed)
{
    if (!gui::begin_menu("Game"))
    {
        return;
    }
    for (const std::string& name : selection::creatable_games())
    {
        if (gui::menu_item(name, board != nullptr && name == board->game_name()).clicked)
        {
            request_switch(state, board, feed, name, std::string());
        }
    }
    gui::end_menu();
}

void opponent_menu(SwitchState& state, const BoardLayer* board, DashboardFeed& feed)
{
    if (!gui::begin_menu("Opponent"))
    {
        return;
    }
    if (board != nullptr)
    {
        for (const std::string& name : selection::opponents_for(board->game_name()))
        {
            if (gui::menu_item(name, name == board->opponent_name()).clicked)
            {
                request_switch(state, board, feed, std::string(), name);
            }
        }
    }
    gui::end_menu();
}

void answer_switch(SwitchState& state, DashboardFeed& feed)
{
    if (!state.pending)
    {
        return;
    }
    if (state.confirm_requested)
    {
        state.confirm_requested = false;
        gui::open_modal(k_confirm);
    }
    const std::string& target = state.game.empty() ? state.opponent : state.game;
    const gui::ModalChoice choice = gui::confirm(k_confirm, gui::context().arena().format("Switching to %s ends the current match and its progress is lost.", target.c_str()), "Switch", "Cancel");
    if (choice == gui::ModalChoice::None)
    {
        return;
    }
    state.pending = false;
    if (choice == gui::ModalChoice::Confirmed)
    {
        dispatch_switch(state, feed);
    }
}

} // namespace oasis

#endif
