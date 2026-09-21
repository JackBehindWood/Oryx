#pragma once

#include <pybind11/pybind11.h>

#include "Oryx/Core/Params.h"

namespace oryx::python
{

[[nodiscard]] ParamValue to_param_value(const std::string& entry, const std::string& key, const pybind11::handle& value);
[[nodiscard]] Params to_params(const std::string& entry, const pybind11::kwargs& kwargs);
[[nodiscard]] pybind11::object to_python(const ParamValue& value);
[[nodiscard]] pybind11::list schema_to_python(const ParamSchema& schema);

} // namespace oryx::python
