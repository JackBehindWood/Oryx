#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include <pybind11/stl.h>

#include "Support/PyHandles.h"
#include "Oryx/Scripting/Support/ScriptUtil.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

SharedPtr<PyState> new_initial_state(const PyGame& game)
{
    return create_shared<PyState>(game.get()->new_initial_state());
}

} // namespace

void bind_game(py::module_& module)
{
    py::module_ game_module = module.def_submodule("game", "Games, states and strategies, whether implemented in C++ or in a script.");
    py::class_<PyGame> game_handle(game_module, "GameHandle", "A game owned by the engine, whether it is implemented in C++ or in a script.");
    py::class_<PyState, SharedPtr<PyState>> state_handle(game_module, "StateHandle", "A game state driven by the engine; a state lent to a strategy is only valid during decide().");

    game_handle
        .def("name", [](const PyGame& game) { return game.get()->name(); })
        .def("num_players", [](const PyGame& game) { return game.get()->num_players(); })
        .def("new_initial_state", &new_initial_state)
        .def("__repr__", [](const PyGame& game) { return "<oryx.GameHandle '" + game.get()->name() + "'>"; });

    state_handle
        .def("legal_actions", &PyState::legal_actions)
        .def("apply", &PyState::apply, py::arg("action"))
        .def("undo", &PyState::undo, py::arg("action"))
        .def("current_player", [](const PyState& state) { return state.get().current_player(); })
        .def("is_terminal", [](const PyState& state) { return state.get().is_terminal(); })
        .def("outcome", &PyState::outcome)
        .def("action_to_string", [](const PyState& state, ActionId action) { return state.get().action_to_string(action); }, py::arg("action"));

    py::class_<PyStrategy>(game_module, "StrategyHandle", "A strategy owned by the engine, whether it is implemented in C++ or in a script.");
}

} // namespace oryx::python
