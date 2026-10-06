#pragma once

namespace oryx::python
{

inline constexpr const char* k_language = "python";
inline constexpr const char* k_module_name = "oryx";
// Set on a registered oryx.Game/oryx.Strategy subclass, so the class itself can be passed where a registry name is expected.
inline constexpr const char* k_registered_id_attribute = "_oryx_id";

} // namespace oryx::python
