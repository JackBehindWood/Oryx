#include "oxpch.h"
#include "Support/PyResolve.h"

#include "PyScripted.h"
#include "PythonContext.h"
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

SharedPtr<IGame> resolve_game(const py::object& spec)
{
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

SharedPtr<IStrategy> resolve_strategy(const py::object& spec, const Params& name_params)
{
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
