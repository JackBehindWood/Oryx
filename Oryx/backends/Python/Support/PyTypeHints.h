#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/typing.h>

#include "Oryx/Core/FixedString.h"

// Names the stubs print for arguments that accept several kinds of object.
namespace oryx::python::hints
{

namespace typing = pybind11::typing;

// Any object, printed in signatures as `Name`; pybind11's own TypeVar needs a feature macro Apple Clang does not set.
template<FixedString Name>
class Named : public pybind11::object
{
    PYBIND11_OBJECT_DEFAULT(Named, object, PyObject_Type)
    using object::object;
};

using State = Named<"oryx.game.State">;
using Context = Named<"oryx.game.Context">;

using GameName = Named<"str | type[oryx.game.Game]">;
using StrategyName = Named<"str | type[oryx.game.Strategy]">;
using GameArg = Named<"str | oryx.game.GameHandle | oryx.game.Game | type[oryx.game.Game]">;
using StrategiesArg = Named<"str | oryx.game.StrategyHandle | oryx.game.Strategy | type[oryx.game.Strategy] | collections.abc.Sequence[str | oryx.game.StrategyHandle | oryx.game.Strategy | type[oryx.game.Strategy]]">;
using PathArg = typing::Optional<Named<"str | os.PathLike[str]">>;

using Seed = typing::Optional<pybind11::int_>;
using OptionalAction = typing::Optional<pybind11::int_>;
using AnyDict = typing::Dict<pybind11::str, pybind11::object>;
using OptionalAnyDict = typing::Optional<AnyDict>;
using Factory = Named<"collections.abc.Callable[..., typing.Any]">;

} // namespace oryx::python::hints

namespace pybind11::detail
{

template<oryx::FixedString Name>
struct handle_type_name<oryx::python::hints::Named<Name>>
{
    static constexpr auto name = const_name(Name.value);
};

} // namespace pybind11::detail
