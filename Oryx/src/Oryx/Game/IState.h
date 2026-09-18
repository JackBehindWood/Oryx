#pragma once

#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/Outcome.h"
#include "Oryx/Game/PlayerId.h"

namespace oryx
{

class IState
{
public:
    virtual ~IState() = default;

    virtual ActionList legal_actions() const = 0;

    virtual void apply(ActionId action) = 0;
    virtual void undo(ActionId action) = 0;

    virtual PlayerId current_player() const = 0;

    virtual bool is_terminal() const = 0;
    virtual Outcome outcome() const = 0;

    virtual std::string action_to_string(ActionId action) const = 0;
};

} // namespace oryx
