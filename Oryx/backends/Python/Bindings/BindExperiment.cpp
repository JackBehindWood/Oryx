#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include <pybind11/stl.h>

#include "Interop/PyHolder.h"
#include "Support/PyParams.h"
#include "Support/PyResolve.h"
#include "Support/PyTypeHints.h"
#include "Support/PyUtil.h"
#include "Oryx/Scripting/Support/InitGuard.h"
#include "Oryx/Scripting/Support/ScriptUtil.h"
#include "Oryx/Simulation/ExperimentIO.h"
#include "Oryx/Simulation/Statistics.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

using ProgressArg = hints::Named<"collections.abc.Callable[[int, int], typing.Any] | None">;
using StrategyArg = hints::Named<"str | type[oryx.game.Strategy] | tuple[str | type[oryx.game.Strategy], dict[str, typing.Any]]">;
using MatchupArg = hints::Named<"dict[str, typing.Any]">;

struct PyExperiment
{
    ExperimentSpec spec;
};

struct PyTournament : PyExperiment
{
};

std::string registry_id(const py::handle& value, const char* what)
{
    py::object resolved = registry_name_of(py::reinterpret_borrow<py::object>(value));
    if (!py::isinstance<py::str>(resolved))
    {
        throw ExperimentError(std::string("an experiment names its ") + what + " by registry id (or a registered class); got " + type_name_of(value));
    }
    return resolved.cast<std::string>();
}

Params params_of(const std::string& entry, const py::handle& value)
{
    if (value.is_none())
    {
        return {};
    }
    return to_params(entry, py::reinterpret_borrow<py::kwargs>(value));
}

StrategySpec to_strategy_spec(const py::handle& item)
{
    if (PyTuple_Check(item.ptr()) && py::len(item) == 2)
    {
        std::string id = registry_id(item[py::int_(0)], "strategy");
        return StrategySpec{ id, params_of(id, item[py::int_(1)]) };
    }
    return StrategySpec{ registry_id(item, "strategy"), {} };
}

std::vector<StrategySpec> to_strategy_specs(const py::handle& items)
{
    std::vector<StrategySpec> specs;
    for (const py::handle& item : items)
    {
        specs.push_back(to_strategy_spec(item));
    }
    return specs;
}

Matchup to_matchup(const py::handle& item)
{
    py::dict entry = py::reinterpret_borrow<py::dict>(item);
    Matchup matchup;
    matchup.game = registry_id(entry["game"], "game");
    matchup.game_params = params_of(matchup.game, entry.attr("get")("game_params", py::none()));
    matchup.seats = to_strategy_specs(entry["strategies"]);
    matchup.label = entry.attr("get")("label", "").cast<std::string>();
    return matchup;
}

ExperimentSpec make_spec(const std::string& name, std::vector<Matchup> matchups, int32_t matches, int32_t repeats, int64_t seed)
{
    ExperimentSpec spec;
    spec.name = name;
    spec.matchups = std::move(matchups);
    spec.matches_per_trial = matches;
    spec.repeats = repeats;
    spec.master_seed = static_cast<uint64_t>(seed);
    return spec;
}

SharedPtr<PyExperiment> make_experiment(const py::sequence& matchups, const std::string& name, int32_t matches, int32_t repeats, int64_t seed)
{
    std::vector<Matchup> parsed;
    for (const py::handle& item : matchups)
    {
        parsed.push_back(to_matchup(item));
    }
    return create_shared<PyExperiment>(PyExperiment{ make_spec(name, std::move(parsed), matches, repeats, seed) });
}

SharedPtr<PyExperiment> make_sweep(const py::object& game, const py::object& strategies, const py::dict& axes, const py::object& game_params, const std::string& name, int32_t matches, int32_t repeats, int64_t seed)
{
    Matchup base;
    base.game = registry_id(game, "game");
    base.game_params = params_of(base.game, game_params);
    base.seats = to_strategy_specs(strategies);

    std::vector<SweepAxis> parsed;
    for (const auto& axis : axes)
    {
        std::string path = py::str(axis.first);
        SweepAxis sweep_axis{ path, {} };
        for (const py::handle& value : axis.second)
        {
            sweep_axis.values.push_back(to_param_value(path, path, value));
        }
        parsed.push_back(std::move(sweep_axis));
    }
    return create_shared<PyExperiment>(PyExperiment{ make_spec(name, sweep(base, parsed), matches, repeats, seed) });
}

SharedPtr<PyTournament> make_tournament(const py::object& game, const py::sequence& strategies, bool rotate_seats, const py::object& game_params, const std::string& name, int32_t matches, int32_t repeats, int64_t seed)
{
    std::string id = registry_id(game, "game");
    PyTournament tournament;
    tournament.spec = make_spec(name, round_robin(id, params_of(id, game_params), to_strategy_specs(strategies), rotate_seats), matches, repeats, seed);
    return create_shared<PyTournament>(std::move(tournament));
}

py::dict params_to_dict(const Params& params)
{
    py::dict values;
    for (const auto& [key, value] : params)
    {
        values[py::str(key)] = to_python(value);
    }
    return values;
}

py::dict spec_to_dict(const ExperimentSpec& spec)
{
    py::list matchups;
    for (const Matchup& matchup : spec.matchups)
    {
        py::list strategies;
        for (const StrategySpec& seat : matchup.seats)
        {
            py::dict strategy;
            strategy["id"] = seat.id;
            strategy["params"] = params_to_dict(seat.params);
            strategies.append(strategy);
        }
        py::dict entry;
        entry["key"] = matchup_key(matchup);
        entry["label"] = matchup.label;
        entry["game"] = matchup.game;
        entry["game_params"] = params_to_dict(matchup.game_params);
        entry["strategies"] = strategies;
        matchups.append(entry);
    }
    py::dict values;
    values["name"] = spec.name;
    values["matches_per_trial"] = spec.matches_per_trial;
    values["repeats"] = spec.repeats;
    values["seed"] = static_cast<int64_t>(spec.master_seed);
    values["matchups"] = matchups;
    return values;
}

py::dict metadata_to_dict(const Metadata& metadata)
{
    py::dict values;
    values["oryx_version"] = metadata.build.version;
    values["git_hash"] = metadata.build.git_hash;
    values["profile"] = metadata.build.profile;
    values["compiler"] = metadata.build.compiler;
    values["platform"] = metadata.build.platform;
    values["spec_hash"] = std::to_string(metadata.spec_hash);
    values["seed"] = static_cast<int64_t>(metadata.master_seed);
    values["timestamp"] = metadata.timestamp.empty() ? py::object(py::none()) : py::object(py::str(metadata.timestamp));
    return values;
}

py::list trials_to_list(const ExperimentResult& result)
{
    py::list trials;
    for (const TrialResult& trial : result.trials)
    {
        py::dict entry;
        entry["matchup"] = trial.matchup;
        entry["repeat"] = trial.repeat;
        entry["metrics"] = trial.metrics.values;
        trials.append(entry);
    }
    return trials;
}

py::list summary_to_list(const ExperimentResult& result)
{
    py::list rows;
    for (const Matchup& matchup : result.spec.matchups)
    {
        std::string key = matchup_key(matchup);
        BatchResult totals = to_batch_result(aggregate(result, key), matchup.seats.size());
        std::vector<int32_t> wins(totals.wins.begin(), totals.wins.end());
        std::vector<double> rates;
        std::vector<std::vector<double>> intervals;
        for (size_t seat = 0; seat < matchup.seats.size(); ++seat)
        {
            rates.push_back(win_rate(totals, static_cast<PlayerId>(seat)));
            Interval interval = wilson_interval(totals.wins[seat], totals.matches);
            intervals.push_back({ interval.lower, interval.upper });
        }
        py::dict row;
        row["matchup"] = key;
        row["matches"] = totals.matches;
        row["wins"] = wins;
        row["draws"] = totals.draws;
        row["win_rates"] = rates;
        row["win_rate_intervals"] = intervals;
        row["draw_rate"] = draw_rate(totals);
        rows.append(row);
    }
    return rows;
}

py::dict to_dict(const ExperimentResult& result)
{
    py::dict values;
    values["spec"] = spec_to_dict(result.spec);
    values["metadata"] = metadata_to_dict(result.metadata);
    values["complete"] = is_complete(result);
    values["trials"] = trials_to_list(result);
    values["summary"] = summary_to_list(result);
    return values;
}

py::object to_dataframe(const ExperimentResult& result)
{
    py::module_ pandas = import_optional("pandas", "ExperimentResult.to_dataframe()");
    std::vector<std::string> matchups;
    std::vector<int32_t> repeats;
    std::vector<std::string> metrics;
    std::vector<double> values;
    for (const TrialResult& trial : result.trials)
    {
        for (const auto& [metric, value] : trial.metrics.values)
        {
            matchups.push_back(trial.matchup);
            repeats.push_back(trial.repeat);
            metrics.push_back(metric);
            values.push_back(value);
        }
    }
    py::dict columns;
    columns["matchup"] = matchups;
    columns["repeat"] = repeats;
    columns["metric"] = metrics;
    columns["value"] = values;
    py::object frame = pandas.attr("DataFrame")(columns);
    py::dict attributes = frame.attr("attrs");
    attributes["name"] = result.spec.name;
    attributes["metadata"] = metadata_to_dict(result.metadata);
    return frame;
}

std::string fixed(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

std::string html_of(const ExperimentResult& result)
{
    std::string html = "<table><caption>oryx.ExperimentResult: " + escape_html(result.spec.name) + ", " + std::to_string(result.trials.size())
        + " trials</caption><thead><tr><th>matchup</th><th>matches</th><th>wins</th><th>draws</th><th>win rates (95% CI)</th></tr></thead><tbody>";
    for (const Matchup& matchup : result.spec.matchups)
    {
        std::string key = matchup_key(matchup);
        BatchResult totals = to_batch_result(aggregate(result, key), matchup.seats.size());
        std::string wins;
        std::string rates;
        for (size_t seat = 0; seat < matchup.seats.size(); ++seat)
        {
            Interval interval = wilson_interval(totals.wins[seat], totals.matches);
            wins += (seat == 0 ? "" : " / ") + std::to_string(totals.wins[seat]);
            rates += (seat == 0 ? "" : " / ") + fixed(win_rate(totals, static_cast<PlayerId>(seat))) + " [" + fixed(interval.lower) + ", " + fixed(interval.upper) + "]";
        }
        html += "<tr><td>" + escape_html(key) + "</td><td>" + std::to_string(totals.matches) + "</td><td>" + wins + "</td><td>" + std::to_string(totals.draws) + "</td><td>" + rates + "</td></tr>";
    }
    return html + "</tbody></table>";
}

std::string repr_of(const ExperimentResult& result)
{
    return "<oryx.ExperimentResult '" + result.spec.name + "' matchups=" + std::to_string(result.spec.matchups.size()) + " trials=" + std::to_string(result.trials.size()) + ">";
}

std::string repr_of(const PyExperiment& experiment)
{
    return "<oryx.Experiment '" + experiment.spec.name + "' matchups=" + std::to_string(experiment.spec.matchups.size()) + ">";
}

// A script-defined game or strategy needs the GIL for every call, so the run keeps it.
bool spec_involves_script(const ExperimentSpec& spec)
{
    for (const Matchup& matchup : spec.matchups)
    {
        UniquePtr<IGame> game = create_game(matchup.game, matchup.game_params);
        std::vector<UniquePtr<IStrategy>> owned;
        std::vector<IStrategy*> strategies;
        for (const StrategySpec& seat : matchup.seats)
        {
            owned.push_back(StrategyRegistry::create(seat.id, seat.params));
            strategies.push_back(owned.back().get());
        }
        if (involves_script(*game, strategies))
        {
            return true;
        }
    }
    return false;
}

// The progress callback is the cancellation point: a KeyboardInterrupt or an error in the user's callback stops the run and is raised afterwards.
ExperimentResult run_with_signals(const ExperimentSpec& spec, const py::object& progress, bool timestamp)
{
    validate(spec);
    bool holds_gil = spec_involves_script(spec);

    std::atomic<bool> cancel{ false };
    SharedPtr<py::error_already_set> failure;
    RunOptions options;
    options.cancel = &cancel;
    options.record_timestamp = timestamp;
    options.progress = [&](int32_t completed, int32_t total)
    {
        py::gil_scoped_acquire acquire;
        try
        {
            if (!progress.is_none())
            {
                progress(completed, total);
            }
            if (PyErr_CheckSignals() != 0)
            {
                throw py::error_already_set();
            }
        }
        catch (const py::error_already_set& error)
        {
            failure = create_shared<py::error_already_set>(error);
            cancel = true;
        }
    };

    ExperimentResult result;
    if (holds_gil)
    {
        result = run_experiment(spec, options);
    }
    else
    {
        py::gil_scoped_release release;
        result = run_experiment(spec, options);
    }
    if (failure)
    {
        throw *failure;
    }
    return result;
}

ExperimentResult run_experiment_py(const PyExperiment& experiment, const ProgressArg& progress, bool timestamp)
{
    return run_with_signals(experiment.spec, progress, timestamp);
}

ExperimentResult rerun_py(const ExperimentResult& result, const ProgressArg& progress, bool timestamp)
{
    return run_with_signals(result.spec, progress, timestamp);
}

void validate_py(const PyExperiment& experiment)
{
    validate(experiment.spec);
}

std::string path_text(const py::object& path)
{
    return py::module_::import("os").attr("fspath")(path).cast<std::string>();
}

ExperimentResult load_py(const hints::Named<"str | os.PathLike[str]">& path)
{
    return load_result(path_text(path));
}

void save_py(const ExperimentResult& result, const hints::Named<"str | os.PathLike[str]">& path)
{
    save_result(result, path_text(path));
}

py::dict cross_table_py(const ExperimentResult& result)
{
    CrossTable table = cross_table(result);
    size_t count = table.strategies.size();
    std::vector<std::vector<double>> scores(count, std::vector<double>(count, 0.0));
    std::vector<std::vector<double>> games(count, std::vector<double>(count, 0.0));
    for (size_t row = 0; row < count; ++row)
    {
        for (size_t column = 0; column < count; ++column)
        {
            scores[row][column] = score(table, row, column);
            games[row][column] = table.games[row * count + column];
        }
    }
    py::dict values;
    values["strategies"] = table.strategies;
    values["scores"] = scores;
    values["games"] = games;
    return values;
}

py::dict ratings_py(const ExperimentResult& result)
{
    CrossTable table = cross_table(result);
    std::vector<double> ratings = bradley_terry(table);
    py::dict values;
    for (size_t strategy = 0; strategy < ratings.size(); ++strategy)
    {
        values[py::str(table.strategies[strategy])] = ratings[strategy];
    }
    return values;
}

py::dict summarize_py(const ExperimentResult& result, const std::string& matchup, const std::string& metric)
{
    Summary summary = summarize(metric_series(result, matchup, metric));
    py::dict values;
    values["count"] = summary.count;
    values["mean"] = summary.mean;
    values["stddev"] = summary.stddev;
    values["ci_half_width"] = summary.ci_half_width;
    return values;
}

py::dict compare_py(const ExperimentResult& result, const std::string& matchup_a, const std::string& matchup_b, const std::string& metric)
{
    Comparison comparison = compare(summarize(metric_series(result, matchup_a, metric)), summarize(metric_series(result, matchup_b, metric)));
    py::dict values;
    values["difference"] = comparison.difference;
    values["lower"] = comparison.interval.lower;
    values["upper"] = comparison.interval.upper;
    values["significant"] = comparison.significant;
    return values;
}

void bind_classes(py::module_& module)
{
    py::class_<PyExperiment, SharedPtr<PyExperiment>>(module, "Experiment", "A set of matchups run for several repeats with seeds derived from the master seed; pure data until run().")
        .def(py::init(OX_GUARDED_FUNC(make_experiment, "oryx.Experiment")), py::arg("matchups"), py::kw_only(), py::arg("name") = "experiment", py::arg("matches") = 100, py::arg("repeats") = 1, py::arg("seed") = 0,
             "Each matchup is a dict with `game`, `strategies` (one entry per seat), and optionally `game_params` and `label`.")
        .def_static("sweep", OX_GUARDED_FUNC(make_sweep, "oryx.Experiment.sweep"), py::arg("game"), py::arg("strategies"), py::arg("axes"), py::kw_only(), py::arg("game_params") = py::none(), py::arg("name") = "sweep", py::arg("matches") = 100, py::arg("repeats") = 1, py::arg("seed") = 0,
                    "One matchup per combination of `axes`, keyed by 'game.<param>' or 'seats.<index>.<param>'.")
        .def_property_readonly("spec", [](const PyExperiment& experiment) { return spec_to_dict(experiment.spec); })
        .def("validate", OX_GUARDED_FUNC(validate_py, "oryx.Experiment.validate"), "Checks ids, parameters and capabilities without running anything.")
        .def("run", OX_GUARDED_FUNC(run_experiment_py, "oryx.Experiment.run"), py::arg("progress") = py::none(), py::kw_only(), py::arg("timestamp") = false,
             "Runs every trial; `progress(done, total)` is called after each, and Ctrl-C stops between trials.")
        .def("__repr__", [](const PyExperiment& experiment) { return repr_of(experiment); });

    py::class_<PyTournament, PyExperiment, SharedPtr<PyTournament>>(module, "Tournament", "A round robin of a strategy pool: every pair plays, with both seatings when `rotate_seats` cancels first-player bias.")
        .def(py::init(OX_GUARDED_FUNC(make_tournament, "oryx.Tournament")), py::arg("game"), py::arg("strategies"), py::kw_only(), py::arg("rotate_seats") = true, py::arg("game_params") = py::none(), py::arg("name") = "tournament", py::arg("matches") = 100, py::arg("repeats") = 1, py::arg("seed") = 0);

    py::class_<ExperimentResult>(module, "ExperimentResult", "The trials of a run with the spec and metadata that produced them.")
        .def_property_readonly("spec", [](const ExperimentResult& result) { return spec_to_dict(result.spec); })
        .def_property_readonly("metadata", [](const ExperimentResult& result) { return metadata_to_dict(result.metadata); })
        .def_property_readonly("complete", &is_complete, "False when the run was cancelled before every trial finished.")
        .def("to_dict", &to_dict)
        .def("to_dataframe", &to_dataframe, "Tidy rows (matchup, repeat, metric, value); needs pandas.")
        .def("summary", [](const ExperimentResult& result) { return summary_to_list(result); }, "Per matchup: totals, win rates with Wilson intervals, draw rate.")
        .def("series", [](const ExperimentResult& result, const std::string& matchup, const std::string& metric) { return metric_series(result, matchup, metric); }, py::arg("matchup"), py::arg("metric"), "One value per repeat.")
        .def("summarize", &summarize_py, py::arg("matchup"), py::arg("metric"), "Mean, stddev and 95% CI half-width of a metric across repeats.")
        .def("compare", &compare_py, py::arg("matchup_a"), py::arg("matchup_b"), py::arg("metric"), "Difference of a metric's mean across repeats between two matchups, with a 95% interval.")
        .def("cross_table", &cross_table_py, "Scores of every strategy against every other (win = 1, draw = 0.5); two-seat matchups only.")
        .def("ratings", &ratings_py, "Bradley-Terry strengths on the Elo scale, centred on 0; two-seat matchups only.")
        .def("save", OX_GUARDED_FUNC(save_py, "oryx.ExperimentResult.save"), py::arg("path"), "Writes result.yaml and trials.csv into the directory `path`.")
        .def_static("load", OX_GUARDED_FUNC(load_py, "oryx.ExperimentResult.load"), py::arg("path"))
        .def("rerun", OX_GUARDED_FUNC(rerun_py, "oryx.ExperimentResult.rerun"), py::arg("progress") = py::none(), py::kw_only(), py::arg("timestamp") = false, "Runs the stored spec again; the trials must match exactly.")
        .def("same_trials", &trials_equal, py::arg("other"), "Whether both results hold identical trials, ignoring order and metadata.")
        .def("_repr_html_", &html_of)
        .def("__repr__", [](const ExperimentResult& result) { return repr_of(result); });
}

} // namespace

void bind_experiment(py::module_& module)
{
    py::module_ experiment = module.def_submodule("experiment", "Describing, running and storing experiments.");
    bind_classes(experiment);
}

} // namespace oryx::python
