#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Game/IState.h"

namespace oryx
{

class IGame
{
public:
    virtual ~IGame() = default;

    virtual UniquePtr<IState> new_initial_state() const = 0;

    virtual std::string name() const = 0;
    virtual int32_t num_players() const = 0;
};

} // namespace oryx
