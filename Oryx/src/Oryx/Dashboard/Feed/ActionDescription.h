#pragma once

#include "Oryx/Dashboard/Feed/DashboardRecord.h"
#include "Oryx/Game/IState.h"

namespace oryx
{

// What a person calls an action and the board spaces it touches, in the order they are picked; spaces are plain indices so Dashboard names no board type.
struct ActionDescription
{
    FixedLabel label;
    uint32_t spaces[k_record_spaces] = {};
    uint8_t space_count = 0;
};

// Supplied by whoever knows the game's vocabulary (the board presenter); the feed calls it at decision time because the state is gone afterwards.
class IActionDescriber
{
public:
    virtual ~IActionDescriber() = default;

    // False leaves `out` unspecified and makes the feed fall back to IState::action_to_string.
    virtual bool describe(const IState& state, ActionId action, ActionDescription& out) const = 0;
};

} // namespace oryx
