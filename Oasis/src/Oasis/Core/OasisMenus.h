#pragma once

#include "Options.h"

#ifdef OX_ENABLE_GRAPHICS

namespace oasis
{

// A request to change game or opponent that waits for the person to confirm when a match is under way.
struct SwitchState
{
    bool pending = false;
    // The confirm dialog still has to be opened (ids are scoped, so that happens at the top level, not inside the menu that asked).
    bool confirm_requested = false;
    std::string game;
    std::string opponent;
};

// Game and Opponent menus of the menu bar being built: a pick asks for a switch, which starts at once without a move played and otherwise waits in `state` for answer_switch.
void game_menu(SwitchState& state, const oryx::BoardLayer* board, oryx::DashboardFeed& feed);
void opponent_menu(SwitchState& state, const oryx::BoardLayer* board, oryx::DashboardFeed& feed);
// Draws the confirm dialog while a switch waits and carries out the answer; call at the top level of the frame.
void answer_switch(SwitchState& state, oryx::DashboardFeed& feed);

} // namespace oasis

#endif
