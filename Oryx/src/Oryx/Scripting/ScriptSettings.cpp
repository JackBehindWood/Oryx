#include "oxpch.h"
#include "Oryx/Scripting/ScriptSettings.h"

namespace oryx
{

void read_settings(ScriptSettings& settings, const SettingsNode& node)
{
    settings.roots = node.paths("roots");
}

} // namespace oryx

OX_REGISTER_SETTINGS(oryx::ScriptSettings, "scripting")
