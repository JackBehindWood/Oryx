#include "oxpch.h"
#include "BindOryx.h"

#include "PythonSupport.h"

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
    py::class_<PyGame>(module, "GameHandle", "A game owned by the engine, whether it is implemented in C++ or in a script.")
        .def("name", [](const PyGame& game) { return game.get()->name(); })
        .def("num_players", [](const PyGame& game) { return game.get()->num_players(); })
        .def("new_initial_state", &new_initial_state)
        .def("__repr__", [](const PyGame& game) { return "<oryx.GameHandle '" + game.get()->name() + "'>"; });

    py::class_<PyState, SharedPtr<PyState>>(module, "StateHandle", "A game state driven by the engine; a state lent to a strategy is only valid during decide().")
        .def("legal_actions", &PyState::legal_actions)
        .def("apply", &PyState::apply, py::arg("action"))
        .def("undo", &PyState::undo, py::arg("action"))
        .def("current_player", [](const PyState& state) { return state.get().current_player(); })
        .def("is_terminal", [](const PyState& state) { return state.get().is_terminal(); })
        .def("outcome", &PyState::outcome)
        .def("action_to_string", [](const PyState& state, ActionId action) { return state.get().action_to_string(action); }, py::arg("action"));

    py::class_<PyStrategy>(module, "StrategyHandle", "A strategy owned by the engine, whether it is implemented in C++ or in a script.");
}

} // namespace oryx::python
