#pragma once

#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/Outcome.h"

namespace oryx
{

class IState
{
public:
    virtual ~IState() = default;

    virtual std::vector<ActionId> legal_actions() const = 0;

    virtual void apply(ActionId action) = 0;
    virtual void undo(ActionId action) = 0;

    virtual int32_t current_player() const = 0;

    virtual bool is_terminal() const = 0;
    virtual Outcome outcome() const = 0;

    virtual std::string action_to_string(ActionId action) const = 0;
};

} // namespace oryx
