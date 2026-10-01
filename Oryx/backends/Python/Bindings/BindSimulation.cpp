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
    PyMatch(SharedPtr<IGame> game, Strategies strategies, bool trace)
        : m_game(std::move(game))
        , m_strategies(std::move(strategies))
        , m_raw(raw_pointers(m_strategies))
        , m_holds_gil(holds_gil_for(*m_game, m_raw))
        , m_match(*m_game, to_small_vector(m_raw))
    {
        if (trace)
        {
            m_match.set_observer(&m_trace);
        }
    }

    [[nodiscard]] Match& match() { return m_match; }
    [[nodiscard]] const std::vector<TraceEntry>& trace() const { return m_trace.entries(); }

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
    TraceRecorder m_trace;
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
        return PyBatchResult{ run_interruptible(m_runner, match_count, m_holds_gil), {}, false, {}, false };
    }

    // One match at a time so each gets its own recorder.
    PyBatchResult run_traced(int32_t match_count)
    {
        if (match_count < 0)
        {
            throw Error("the number of matches cannot be negative");
        }

        PyBatchResult result{ m_runner.run(0), {}, false, {}, true };
        for (int32_t i = 0; i < match_count; ++i)
        {
            TraceRecorder recorder;
            m_runner.set_observer(&recorder);
            BatchResult piece;
            try
            {
                piece = run_once();
            }
            catch (...)
            {
                m_runner.set_observer(nullptr);
                throw;
            }
            m_runner.set_observer(nullptr);
            merge(result.counts, piece);
            result.trace.push_back(recorder.entries());
            if (PyErr_CheckSignals() != 0)
            {
                throw py::error_already_set();
            }
        }
        return result;
    }

private:
    BatchResult run_once()
    {
        if (m_holds_gil)
        {
            return m_runner.run(1);
        }
        py::gil_scoped_release release;
        return m_runner.run(1);
    }

    SharedPtr<IGame> m_game;
    Strategies m_strategies;
    std::vector<IStrategy*> m_raw;
    bool m_holds_gil;
    BatchRunner m_runner;
};

SharedPtr<PyMatch> make_match(const hints::GameArg& game, const hints::StrategiesArg& strategies, bool trace)
{
    SharedPtr<IGame> resolved = resolve_game(game);
    return create_shared<PyMatch>(resolved, resolve_strategies(strategy_specs(strategies, resolved->num_players())), trace);
}

SharedPtr<PyBatchRunner> make_batch_runner(const hints::GameArg& game, const hints::StrategiesArg& strategies)
{
    SharedPtr<IGame> resolved = resolve_game(game);
    return create_shared<PyBatchRunner>(resolved, resolve_strategies(strategy_specs(strategies, resolved->num_players())));
}

PyBatchResult simulate(const hints::GameArg& game, const hints::StrategiesArg& strategies, int32_t games, const hints::Seed& seed, bool trace)
{
    SharedPtr<IGame> resolved = resolve_game(game);
    py::list specs = strategy_specs(strategies, resolved->num_players());
    PyBatchRunner runner(resolved, resolve_seeded_strategies(specs, seed));
    PyBatchResult result = trace ? runner.run_traced(games) : runner.run(games);
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
        .def(py::init(OX_GUARDED_FUNC(make_match, "oryx.Match")), py::arg("game"), py::arg("strategies"), py::kw_only(), py::arg("trace") = false)
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
        .def_property_readonly("trace", [](const PyMatch& match) { return match.trace(); }, "The decisions made so far when created with trace=True, else empty; undo() does not remove them.")
        .def("history", &history_of);
}

void bind_batch(py::module_& module)
{
    py::class_<PyBatchRunner, SharedPtr<PyBatchRunner>>(module, "BatchRunner", "Plays many matches of one game between the same strategies.")
        .def(py::init(OX_GUARDED_FUNC(make_batch_runner, "oryx.BatchRunner")), py::arg("game"), py::arg("strategies"))
        .def("run", &PyBatchRunner::run, py::arg("matches"));

    module.def("simulate", OX_GUARDED_FUNC(simulate, "oryx.simulate"),
               py::arg("game"), py::arg("strategies"), py::arg("games") = 1000, py::arg("seed") = py::none(), py::kw_only(), py::arg("trace") = false,
               "Plays `games` matches; strategies created by name that take a `seed` get seed + seat index. `trace=True` keeps every decision in `result.trace`, one list per match.");
}

} // namespace

void bind_simulation(py::module_& module)
{
    py::module_ simulation = module.def_submodule("simulation", "Playing matches and batches of matches.");
    bind_match(simulation);
    bind_batch(simulation);
}

} // namespace oryx::python
