#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include <pybind11/stl.h>

#include "Support/PyBatch.h"
#include "Support/PyHandles.h"
#include "Oryx/Scripting/Support/ScriptUtil.h"
#include "Support/PyResolve.h"
#include "Support/PyTypeHints.h"
#include "Support/PyUtil.h"
#include "Oryx/Scripting/Support/InitGuard.h"
#include "Oryx/Simulation/BatchRunner.h"
#include "Oryx/Simulation/Match.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

class PyMatch
{
public:
    PyMatch(SharedPtr<IGame> game, Strategies strategies)
        : m_game(std::move(game))
        , m_strategies(std::move(strategies))
        , m_raw(raw_pointers(m_strategies))
        , m_holds_gil(holds_gil_for(*m_game, m_raw))
        , m_match(*m_game, to_small_vector(m_raw))
    {
    }

    [[nodiscard]] Match& match() { return m_match; }

    std::vector<double> play()
    {
        if (m_holds_gil)
        {
            return rewards_to_vector(m_match.play().rewards);
        }
        py::gil_scoped_release release;
        return rewards_to_vector(m_match.play().rewards);
    }

private:
    SharedPtr<IGame> m_game;
    Strategies m_strategies;
    std::vector<IStrategy*> m_raw;
    bool m_holds_gil;
    Match m_match;
};

class PyBatchRunner
{
public:
    PyBatchRunner(SharedPtr<IGame> game, Strategies strategies)
        : m_game(std::move(game))
        , m_strategies(std::move(strategies))
        , m_raw(raw_pointers(m_strategies))
        , m_holds_gil(holds_gil_for(*m_game, m_raw))
        , m_runner(*m_game, to_small_vector(m_raw))
    {
    }

    [[nodiscard]] const IGame& game() const { return *m_game; }

    PyBatchResult run(int32_t match_count)
    {
        return PyBatchResult{ run_interruptible(m_runner, match_count, m_holds_gil), {}, false };
    }

private:
    SharedPtr<IGame> m_game;
    Strategies m_strategies;
    std::vector<IStrategy*> m_raw;
    bool m_holds_gil;
    BatchRunner m_runner;
};

SharedPtr<PyMatch> make_match(const hints::GameArg& game, const hints::StrategiesArg& strategies)
{
    SharedPtr<IGame> resolved = resolve_game(game);
    return create_shared<PyMatch>(resolved, resolve_strategies(strategy_specs(strategies, resolved->num_players())));
}

SharedPtr<PyBatchRunner> make_batch_runner(const hints::GameArg& game, const hints::StrategiesArg& strategies)
{
    SharedPtr<IGame> resolved = resolve_game(game);
    return create_shared<PyBatchRunner>(resolved, resolve_strategies(strategy_specs(strategies, resolved->num_players())));
}

PyBatchResult simulate(const hints::GameArg& game, const hints::StrategiesArg& strategies, int32_t games, const hints::Seed& seed)
{
    SharedPtr<IGame> resolved = resolve_game(game);
    py::list specs = strategy_specs(strategies, resolved->num_players());
    PyBatchRunner runner(resolved, resolve_seeded_strategies(specs, seed));
    PyBatchResult result = runner.run(games);
    result.has_metadata = true;
    result.metadata = make_metadata(runner.game(), specs, games, seed);
    return result;
}

hints::OptionalAction action_or_none(ActionId action)
{
    return hints::OptionalAction(is_valid(action) ? py::cast(action) : py::none());
}

std::vector<ActionId> history_of(PyMatch& match)
{
    std::span<const ActionId> actions = match.match().history().actions();
    return std::vector<ActionId>(actions.begin(), actions.end());
}

void bind_match(py::module_& module)
{
    py::class_<PyMatch, SharedPtr<PyMatch>>(module, "Match", "One game between strategies, stepped by hand or played to the end.")
        .def(py::init(OX_GUARDED_FUNC(make_match, "oryx.Match")), py::arg("game"), py::arg("strategies"))
        .def("state", [](PyMatch& match) { return create_shared<PyState>(match.match().state(), nullptr, StateAccess::ReadOnly); }, py::keep_alive<0, 1>())
        .def("is_terminal", [](PyMatch& match) { return match.match().is_terminal(); })
        .def("current_player", [](PyMatch& match) { return match.match().current_player(); })
        .def("outcome", [](PyMatch& match) { return rewards_to_vector(match.match().outcome().rewards); })
        .def("decide", [](PyMatch& match) { return match.match().decide(); })
        .def("apply", [](PyMatch& match, ActionId action)
            {
                check_legal(match.match().state(), action);
                match.match().apply(action);
            }, py::arg("action"))
        .def("undo", [](PyMatch& match) { return action_or_none(match.match().undo()); })
        .def("redo", [](PyMatch& match) { return action_or_none(match.match().redo()); })
        .def("play", &PyMatch::play)
        .def("history", &history_of);
}

void bind_batch(py::module_& module)
{
    py::class_<PyBatchRunner, SharedPtr<PyBatchRunner>>(module, "BatchRunner", "Plays many matches of one game between the same strategies.")
        .def(py::init(OX_GUARDED_FUNC(make_batch_runner, "oryx.BatchRunner")), py::arg("game"), py::arg("strategies"))
        .def("run", &PyBatchRunner::run, py::arg("matches"));

    module.def("simulate", OX_GUARDED_FUNC(simulate, "oryx.simulate"),
               py::arg("game"), py::arg("strategies"), py::arg("games") = 1000, py::arg("seed") = py::none(),
               "Plays `games` matches; strategies created by name that take a `seed` get seed + seat index.");
}

} // namespace

void bind_simulation(py::module_& module)
{
    py::module_ simulation = module.def_submodule("simulation", "Playing matches and batches of matches.");
    bind_match(simulation);
    bind_batch(simulation);
}

} // namespace oryx::python
