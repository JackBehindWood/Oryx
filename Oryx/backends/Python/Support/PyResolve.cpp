#include "oxpch.h"
#include "Support/PyResolve.h"

#include "PyScripted.h"
#include "PythonContext.h"
#include "PythonLanguage.h"
#include "Support/PyHandles.h"
#include "Support/PySchema.h"
#include "Support/PyUtil.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

SharedPtr<const ScriptOrigin> shared_origin_of(const py::object& instance)
{
    return create_shared<const ScriptOrigin>(origin_of_class(py::type::of(instance)));
}

} // namespace

SharedPtr<IGame> adapt_game(const py::object& instance)
{
    return create_shared<PyScriptedGame>(PyRef::borrow(instance.ptr()), shared_origin_of(instance), schema_of_class(py::type::of(instance)));
}

SharedPtr<IStrategy> adapt_strategy(const py::object& instance)
{
    return create_shared<PyScriptedStrategy>(PyRef::borrow(instance.ptr()), shared_origin_of(instance));
}

bool is_script_game(const py::handle& value)
{
    return PythonContext::current().is_game(value.ptr());
}

bool is_script_strategy(const py::handle& value)
{
    return PythonContext::current().is_strategy(value.ptr());
}

py::object registry_name_of(const py::object& spec)
{
    const PythonContext& context = PythonContext::current();
    if (!context.is_game_class(spec.ptr()) && !context.is_strategy_class(spec.ptr()))
    {
        return spec;
    }

    py::object own = spec.attr("__dict__");
    if (!own.contains(kRegisteredIdAttribute))
    {
        std::string name = spec.attr("__name__").cast<std::string>();
        throw Error("the class " + name + " is not registered; give it an id (`class " + name + "(..., id=\"...\")`) to pass the class itself");
    }
    return own[kRegisteredIdAttribute];
}

py::list strategy_specs(const py::object& spec, int32_t seats)
{
    py::list specs;
    if (PyList_Check(spec.ptr()) || PyTuple_Check(spec.ptr()))
    {
        for (const py::handle& entry : spec)
        {
            specs.append(registry_name_of(py::reinterpret_borrow<py::object>(entry)));
        }
        return specs;
    }

    py::object single = registry_name_of(spec);
    for (int32_t seat = 0; seat < seats; ++seat)
    {
        specs.append(single);
    }
    return specs;
}

SharedPtr<IGame> resolve_game(const py::object& game_spec)
{
    py::object spec = registry_name_of(game_spec);
    if (py::isinstance<py::str>(spec))
    {
        std::string name = spec.cast<std::string>();
        SharedPtr<IGame> game = create_game(name);
        if (!game)
        {
            throw Error("no game is registered as '" + name + "'");
        }
        return game;
    }
    if (py::isinstance<PyGame>(spec))
    {
        return spec.cast<const PyGame&>().get();
    }
    if (is_script_game(spec))
    {
        return adapt_game(spec);
    }
    throw Error("expected a game name or a game, got '" + type_name_of(spec) + "'");
}

SharedPtr<IStrategy> resolve_strategy(const py::object& strategy_spec, const Params& name_params)
{
    py::object spec = registry_name_of(strategy_spec);
    if (py::isinstance<py::str>(spec))
    {
        std::string name = spec.cast<std::string>();
        SharedPtr<IStrategy> strategy = StrategyRegistry::create(name, name_params);
        if (!strategy)
        {
            throw Error("no strategy is registered as '" + name + "'");
        }
        return strategy;
    }
    if (py::isinstance<PyStrategy>(spec))
    {
        return spec.cast<const PyStrategy&>().get();
    }
    if (is_script_strategy(spec))
    {
        return adapt_strategy(spec);
    }
    throw Error("expected a strategy name or a strategy, got '" + type_name_of(spec) + "'");
}

} // namespace oryx::python
