#include "oxpch.h"
#include "Bindings/BindOryx.h"

#include <pybind11/stl.h>

#include "Support/PyHandles.h"
#include "Support/PyParams.h"
#include "Support/PyResolve.h"
#include "Support/PyUtil.h"
#include "Support/PyTypeHints.h"
#include "Oryx/Scripting/Support/InitGuard.h"
#include "Oryx/Scripting/Registry/ScriptRegistry.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

std::string joined(std::vector<std::string> names)
{
    std::sort(names.begin(), names.end());
    std::string result;
    for (const std::string& name : names)
    {
        result += (result.empty() ? "'" : ", '") + name + "'";
    }
    return result.empty() ? "none" : result;
}

template<typename Entry>
[[noreturn]] void throw_unknown(const char* kind, const std::string& name)
{
    throw Error(std::string("no ") + kind + " is registered as '" + name + "' (registered: " + joined(Registry<Entry>::names()) + ")");
}

std::string entry_name(const py::object& spec)
{
    py::object name = registry_name_of(spec);
    if (!py::isinstance<py::str>(name))
    {
        throw Error("expected a registry name or a registered class, got '" + type_name_of(name) + "'");
    }
    return name.cast<std::string>();
}

PyGame make_game(const hints::GameName& spec, const py::kwargs& kwargs)
{
    std::string name = entry_name(spec);
    if (!GameRegistry::has(name))
    {
        throw_unknown<IGame>("game", name);
    }
    return PyGame(create_game(name, to_params(name, kwargs)));
}

PyStrategy make_strategy(const hints::StrategyName& spec, const py::kwargs& kwargs)
{
    std::string name = entry_name(spec);
    if (!StrategyRegistry::has(name))
    {
        throw_unknown<IStrategy>("strategy", name);
    }
    return PyStrategy(StrategyRegistry::create(name, to_params(name, kwargs)));
}

template<typename Entry>
std::vector<std::string> list_entries()
{
    std::vector<std::string> names = Registry<Entry>::names();
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> list_games() { return list_entries<IGame>(); }
std::vector<std::string> list_strategies() { return list_entries<IStrategy>(); }

py::object origin_to_python(const ScriptOrigin* origin)
{
    if (origin == nullptr)
    {
        return py::none();
    }
    py::dict result;
    result["language"] = origin->language;
    result["module"] = origin->module;
    result["source_file"] = origin->source_file;
    return result;
}

template<typename Entry>
py::dict describe_entry(const char* kind, const std::string& name, const ScriptOrigin* origin)
{
    const EntryInfo* info = Registry<Entry>::info(name);
    if (info == nullptr)
    {
        throw_unknown<Entry>(kind, name);
    }

    py::dict result;
    result["name"] = name;
    result["description"] = info->description;
    result["params"] = schema_to_python(info->schema);
    result["origin"] = origin_to_python(origin);
    return result;
}

hints::AnyDict describe_game(const hints::GameName& spec)
{
    std::string name = entry_name(spec);
    return hints::AnyDict(describe_entry<IGame>("game", name, scripted_game_origin(name)));
}
hints::AnyDict describe_strategy(const hints::StrategyName& spec)
{
    std::string name = entry_name(spec);
    return hints::AnyDict(describe_entry<IStrategy>("strategy", name, scripted_strategy_origin(name)));
}

} // namespace

void bind_registry(py::module_& module)
{
    py::module_ registry = module.def_submodule("registry", "Creating, listing, describing and registering games and strategies by name.");
    registry.def("make_game", OX_GUARDED_FUNC(make_game, "oryx.make_game"), py::arg("name"), "Creates a registered game; keyword arguments are its parameters.");
    registry.def("make_strategy", OX_GUARDED_FUNC(make_strategy, "oryx.make_strategy"), py::arg("name"), "Creates a registered strategy; keyword arguments are its parameters.");
    registry.def("list_games", OX_GUARDED_FUNC(list_games, "oryx.list_games"));
    registry.def("list_strategies", OX_GUARDED_FUNC(list_strategies, "oryx.list_strategies"));
    registry.def("describe_game", OX_GUARDED_FUNC(describe_game, "oryx.describe_game"), py::arg("name"));
    registry.def("describe_strategy", OX_GUARDED_FUNC(describe_strategy, "oryx.describe_strategy"), py::arg("name"));
}

} // namespace oryx::python
