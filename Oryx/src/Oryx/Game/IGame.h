#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Registry.h"
#include "Oryx/Game/IActionFeatures.h"
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

    // Optional capability exposing this game's IActionFeatures (see
    // Oryx/Game/IActionFeatures.h). nullptr when the game doesn't provide one.
    virtual IActionFeatures* action_features() const { return nullptr; }
};

using GameRegistry = Registry<IGame>;

inline UniquePtr<IGame> create_game(const std::string& name, const Params& params = {})
{
    return GameRegistry::create(name, params);
}

} // namespace oryx
