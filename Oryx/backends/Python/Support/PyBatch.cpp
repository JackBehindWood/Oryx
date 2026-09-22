#include "oxpch.h"
#include "Support/PyBatch.h"

#include "Support/PyResolve.h"
#include "Oryx/Scripting/Support/ScriptUtil.h"
#include "Oryx/Simulation/Match.h"
#include "Support/PyUtil.h"

namespace py = pybind11;

namespace oryx::python
{

Strategies resolve_strategies(const py::sequence& specs)
{
    Strategies strategies;
    for (const py::handle& spec : specs)
    {
        strategies.push_back(resolve_strategy(py::reinterpret_borrow<py::object>(spec)));
    }
    return strategies;
}

std::vector<IStrategy*> raw_pointers(const Strategies& strategies)
{
    std::vector<IStrategy*> result;
    for (const SharedPtr<IStrategy>& strategy : strategies)
    {
        result.push_back(strategy.get());
    }
    return result;
}

SmallVector<IStrategy*, 2> to_small_vector(const std::vector<IStrategy*>& strategies)
{
    SmallVector<IStrategy*, 2> result;
    for (IStrategy* strategy : strategies)
    {
        result.push_back(strategy);
    }
    return result;
}

bool holds_gil_for(const IGame& game, const std::vector<IStrategy*>& strategies)
{
    if (static_cast<int32_t>(strategies.size()) != game.num_players())
    {
        throw Error(game.name() + " has " + std::to_string(game.num_players()) + " players but " + std::to_string(strategies.size()) + " strategies were given");
    }

    UniquePtr<IState> probe = game.new_initial_state();
    Context context = Match::build_context(game, *probe);
    for (const IStrategy* strategy : strategies)
    {
        if (!Match::missing_capabilities(*strategy, context).empty())
        {
            throw Error("a strategy requires a capability that " + game.name() + " does not provide");
        }
    }
    return involves_script(game, strategies);
}

Strategies resolve_seeded_strategies(const py::sequence& specs, const py::object& seed)
{
    Strategies strategies;
    size_t seat = 0;
    for (const py::handle& handle : specs)
    {
        py::object spec = py::reinterpret_borrow<py::object>(handle);
        Params params;
        if (!seed.is_none() && py::isinstance<py::str>(spec))
        {
            params = seeded_params(spec.cast<std::string>(), seed.cast<int64_t>(), seat);
        }
        strategies.push_back(resolve_strategy(spec, params));
        ++seat;
    }
    return strategies;
}

std::vector<std::string> strategy_labels(const py::sequence& specs)
{
    std::vector<std::string> labels;
    for (const py::handle& spec : specs)
    {
        labels.push_back(py::isinstance<py::str>(spec) ? spec.cast<std::string>() : type_name_of(spec));
    }
    return labels;
}

RunMetadata make_metadata(const IGame& game, const py::sequence& strategies, int32_t games, const py::object& seed)
{
    return RunMetadata{ game.name(), strategy_labels(strategies), games, !seed.is_none(), seed.is_none() ? 0 : seed.cast<int64_t>() };
}

BatchResult run_interruptible(BatchRunner& runner, int32_t match_count, bool holds_gil)
{
    if (match_count < 0)
    {
        throw Error("the number of matches cannot be negative");
    }

    BatchResult total = runner.run(0); // sized, zero matches - also match_count == 0's whole answer
    int32_t remaining = match_count;
    int32_t chunk = std::min(remaining, 1);
    constexpr double kTargetSeconds = 0.05;

    while (remaining > 0)
    {
        chunk = std::min(chunk, remaining);
        BatchResult piece;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        if (holds_gil)
        {
            piece = runner.run(chunk);
        }
        else
        {
            py::gil_scoped_release release;
            piece = runner.run(chunk);
        }
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        merge(total, piece);
        remaining -= chunk;

        // The GIL is held again here (gil_scoped_release's destructor ran at the closing brace
        // above): PyErr_CheckSignals is a Python C-API call and needs it.
        if (PyErr_CheckSignals() != 0)
        {
            throw py::error_already_set();
        }

        double per_match = elapsed / chunk;
        int32_t estimated = per_match > 0.0 ? static_cast<int32_t>(kTargetSeconds / per_match) : chunk * 2;
        chunk = std::clamp(estimated, 1, chunk * 2);
    }
    return total;
}

} // namespace oryx::python
