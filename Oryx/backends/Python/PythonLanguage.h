#pragma once

namespace oryx::python
{

inline constexpr const char* kLanguage = "python";
inline constexpr const char* kModuleName = "oryx";
// Set on a registered oryx.Game/oryx.Strategy subclass, so the class itself can be passed where a registry name is expected.
inline constexpr const char* kRegisteredIdAttribute = "_oryx_id";

} // namespace oryx::python
