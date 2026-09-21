#pragma once

#include <pybind11/pybind11.h>

#include "Oryx/Core/Params.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx::python
{

// Wrap an instance of a class deriving from oryx.Game / oryx.Strategy.
[[nodiscard]] SharedPtr<IGame> adapt_game(const pybind11::object& instance);
[[nodiscard]] SharedPtr<IStrategy> adapt_strategy(const pybind11::object& instance);

[[nodiscard]] bool is_script_game(const pybind11::handle& value);
[[nodiscard]] bool is_script_strategy(const pybind11::handle& value);

// Accepts a registry name, or a native/Python-defined game.
[[nodiscard]] SharedPtr<IGame> resolve_game(const pybind11::object& spec);
// A registry name is created with `name_params`.
[[nodiscard]] SharedPtr<IStrategy> resolve_strategy(const pybind11::object& spec, const Params& name_params = {});

} // namespace oryx::python
