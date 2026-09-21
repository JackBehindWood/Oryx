#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include <pybind11/stl.h>

#include "Support/PyBatch.h"
#include "Oryx/Scripting/Support/ScriptUtil.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

std::vector<int32_t> wins_of(const PyBatchResult& result)
{
    return std::vector<int32_t>(result.counts.wins.begin(), result.counts.wins.end());
}

std::vector<double> win_rates_of(const PyBatchResult& result)
{
    std::vector<double> rates;
    for (size_t player = 0; player < result.counts.wins.size(); ++player)
    {
        rates.push_back(win_rate(result.counts, static_cast<PlayerId>(player)));
    }
    return rates;
}

std::vector<double> mean_rewards_of(const PyBatchResult& result)
{
    std::vector<double> means;
    for (size_t player = 0; player < result.counts.rewards.player_count(); ++player)
    {
        means.push_back(mean_reward(result.counts, static_cast<PlayerId>(player)));
    }
    return means;
}

std::string version_string()
{
    return std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) + "." + std::to_string(VERSION_PATCH);
}

py::object metadata_of(const PyBatchResult& result)
{
    if (!result.has_metadata)
    {
        return py::none();
    }
    py::dict metadata;
    metadata["game"] = result.metadata.game;
    metadata["strategies"] = result.metadata.strategies;
    metadata["games"] = result.metadata.games;
    metadata["seed"] = result.metadata.seeded ? py::cast(result.metadata.seed) : py::none();
    metadata["oryx_version"] = version_string();
    return metadata;
}

py::dict to_dict(const PyBatchResult& result)
{
    py::dict values;
    values["matches"] = result.counts.matches;
    values["wins"] = wins_of(result);
    values["draws"] = result.counts.draws;
    values["rewards"] = rewards_to_vector(result.counts.rewards);
    values["decisions"] = result.counts.decisions;
    values["win_rates"] = win_rates_of(result);
    values["draw_rate"] = draw_rate(result.counts);
    values["mean_rewards"] = mean_rewards_of(result);
    values["metadata"] = metadata_of(result);
    return values;
}

std::string repr_of(const PyBatchResult& result)
{
    std::string wins;
    for (int32_t count : result.counts.wins)
    {
        wins += (wins.empty() ? "" : ", ") + std::to_string(count);
    }
    return "<oryx.BatchResult matches=" + std::to_string(result.counts.matches) + " wins=[" + wins + "] draws=" + std::to_string(result.counts.draws) + ">";
}

std::string escape_html(const std::string& text)
{
    std::string escaped;
    for (char c : text)
    {
        switch (c)
        {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default: escaped += c; break;
        }
    }
    return escaped;
}

std::string fixed(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

std::string caption_of(const PyBatchResult& result)
{
    std::string caption = "oryx.BatchResult: " + std::to_string(result.counts.matches) + " matches, " + std::to_string(result.counts.draws) + " draws";
    if (!result.has_metadata)
    {
        return caption;
    }

    caption += " (" + escape_html(result.metadata.game) + ": ";
    for (size_t seat = 0; seat < result.metadata.strategies.size(); ++seat)
    {
        caption += (seat == 0 ? "" : " vs ") + escape_html(result.metadata.strategies[seat]);
    }
    return caption + (result.metadata.seeded ? ", seed " + std::to_string(result.metadata.seed) : "") + ")";
}

std::string html_of(const PyBatchResult& result)
{
    std::string html = "<table><caption>" + caption_of(result) + "</caption><thead><tr><th>player</th><th>wins</th><th>win rate</th><th>mean reward</th></tr></thead><tbody>";
    std::vector<double> rates = win_rates_of(result);
    std::vector<double> means = mean_rewards_of(result);
    for (size_t player = 0; player < result.counts.wins.size(); ++player)
    {
        html += "<tr><td>" + std::to_string(player) + "</td><td>" + std::to_string(result.counts.wins[player]) + "</td><td>" + fixed(rates[player]) + "</td><td>" + fixed(means[player]) + "</td></tr>";
    }
    return html + "</tbody></table>";
}

py::module_ import_optional(const char* name, const char* method)
{
    try
    {
        return py::module_::import(name);
    }
    catch (const py::error_already_set& error)
    {
        if (!error.matches(PyExc_ImportError))
        {
            throw;
        }
        throw Error(std::string(method) + " needs " + name + "; install it with `pip install " + name + "`");
    }
}

py::dict to_numpy(const PyBatchResult& result)
{
    py::object array = import_optional("numpy", "BatchResult.to_numpy()").attr("array");
    py::dict arrays;
    arrays["wins"] = array(wins_of(result));
    arrays["rewards"] = array(rewards_to_vector(result.counts.rewards));
    arrays["win_rates"] = array(win_rates_of(result));
    arrays["mean_rewards"] = array(mean_rewards_of(result));
    return arrays;
}

py::object to_dataframe(const PyBatchResult& result)
{
    py::module_ pandas = import_optional("pandas", "BatchResult.to_dataframe()");
    std::vector<int32_t> players;
    for (size_t player = 0; player < result.counts.wins.size(); ++player)
    {
        players.push_back(static_cast<int32_t>(player));
    }

    py::dict columns;
    columns["player"] = players;
    columns["wins"] = wins_of(result);
    columns["win_rate"] = win_rates_of(result);
    columns["mean_reward"] = mean_rewards_of(result);
    py::object frame = pandas.attr("DataFrame")(columns);

    py::dict attributes = frame.attr("attrs");
    attributes["matches"] = result.counts.matches;
    attributes["draws"] = result.counts.draws;
    attributes["draw_rate"] = draw_rate(result.counts);
    attributes["decisions"] = result.counts.decisions;
    if (result.has_metadata)
    {
        attributes["metadata"] = metadata_of(result);
    }
    return frame;
}

} // namespace

void bind_results(py::module_& module)
{
    py::module_ results = module.def_submodule("results", "The totals of a batch of matches.");
    py::class_<PyBatchResult>(results, "BatchResult", "Totals from a batch of matches, with derived rates and, for simulate(), what was run.")
        .def_property_readonly("matches", [](const PyBatchResult& result) { return result.counts.matches; })
        .def_property_readonly("wins", &wins_of)
        .def_property_readonly("draws", [](const PyBatchResult& result) { return result.counts.draws; })
        .def_property_readonly("rewards", [](const PyBatchResult& result) { return rewards_to_vector(result.counts.rewards); })
        .def_property_readonly("decisions", [](const PyBatchResult& result) { return result.counts.decisions; })
        .def_property_readonly("win_rates", &win_rates_of)
        .def_property_readonly("draw_rate", [](const PyBatchResult& result) { return draw_rate(result.counts); })
        .def_property_readonly("mean_rewards", &mean_rewards_of)
        .def_property_readonly("metadata", &metadata_of, "game, strategies, games, seed and oryx_version of a simulate() run; None for a BatchRunner.")
        .def("to_dict", &to_dict)
        .def("to_numpy", &to_numpy, "The counts and rates as numpy arrays; needs numpy.")
        .def("to_dataframe", &to_dataframe, "One row per player; needs pandas.")
        .def("_repr_html_", &html_of)
        .def("__repr__", &repr_of);
}

} // namespace oryx::python
