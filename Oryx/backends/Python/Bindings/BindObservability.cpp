#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include <pybind11/stl.h>

#include "Support/PyHandles.h"
#include "Support/PyUtil.h"
#include "Oryx/Strategy/Observability/TraceRecorder.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

py::object action_or_none(ActionId action)
{
    return is_valid(action) ? py::cast(action) : py::none();
}

py::list scores_of(const TraceEntry& entry)
{
    py::list scores;
    for (size_t i = 0; i < entry.decision.scores.size(); ++i)
    {
        const ActionScore& score = entry.decision.scores[i];
        py::dict item;
        item["action"] = score.action;
        item["label"] = entry.score_labels[i];
        item["probability"] = score.has_probability ? py::cast(score.probability) : py::none();
        item["value"] = score.has_value ? py::cast(score.value) : py::none();
        scores.append(item);
    }
    return scores;
}

py::dict extra_of(const TraceEntry& entry)
{
    py::dict extra;
    for (const auto& [key, value] : entry.decision.extra.values)
    {
        extra[py::str(key)] = value;
    }
    return extra;
}

py::list tree_of(const TraceEntry& entry)
{
    py::list tree;
    for (const SearchNode& node : entry.decision.tree)
    {
        py::dict item;
        item["parent"] = node.parent;
        item["action"] = action_or_none(node.action);
        item["visits"] = node.visits;
        item["value"] = node.value;
        tree.append(item);
    }
    return tree;
}

py::dict to_dict(const TraceEntry& entry)
{
    py::dict values;
    values["ply"] = entry.ply;
    values["player"] = entry.decision.player;
    values["chosen"] = action_or_none(entry.decision.chosen);
    values["chosen_label"] = entry.chosen_label;
    values["scores"] = scores_of(entry);
    values["extra"] = extra_of(entry);
    values["tree"] = tree_of(entry);
    return values;
}

std::string repr_of(const TraceEntry& entry)
{
    return "<oryx.Decision ply=" + std::to_string(entry.ply) + " player=" + std::to_string(entry.decision.player) + " chosen=" + entry.chosen_label + ">";
}

} // namespace

void bind_observability(py::module_& module)
{
    py::module_ observability = module.def_submodule("observability", "What a strategy reports about a decision.");

    py::class_<TraceEntry>(observability, "Decision", "One decision of a traced match: who chose what, the strategy's scores and its diagnostics.")
        .def_readonly("ply", &TraceEntry::ply)
        .def_property_readonly("player", [](const TraceEntry& entry) { return entry.decision.player; })
        .def_property_readonly("chosen", [](const TraceEntry& entry) { return entry.decision.chosen; })
        .def_readonly("chosen_label", &TraceEntry::chosen_label)
        .def_property_readonly("scores", &scores_of, "Per action: action, label, probability and value (None when the strategy gave none).")
        .def_property_readonly("extra", &extra_of, "Namespaced diagnostics such as minimax/nodes.")
        .def_property_readonly("tree", &tree_of, "Flat search-tree nodes (parent, action, visits, value); empty unless the strategy builds one.")
        .def("to_dict", &to_dict)
        .def("__repr__", &repr_of);

    py::class_<PyObserver, SharedPtr<PyObserver>>(observability, "Observer", "Where decide() publishes its Decision; only valid until decide() returns.")
        .def("publish", &PyObserver::publish, py::arg("chosen"), py::kw_only(),
             py::arg("probabilities") = std::map<ActionId, double>{}, py::arg("values") = std::map<ActionId, double>{}, py::arg("extra") = std::map<std::string, double>{},
             "Reports the action chosen with optional per-action probabilities and values and namespaced `extra` diagnostics (\"mystrategy/nodes\").");
}

} // namespace oryx::python
