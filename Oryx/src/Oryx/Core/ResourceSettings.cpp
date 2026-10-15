#include "oxpch.h"
#include "Oryx/Core/ResourceSettings.h"

namespace oryx
{

std::filesystem::path compiled_directory(const ResourceSettings& settings)
{
    return settings.root / "compiled";
}

} // namespace oryx

OX_REGISTER_SETTINGS(oryx::ResourceSettings, "resources")
