#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `scripting:` section of the settings file.
struct ScriptSettings
{
    // Whether scripting is active at all; false skips script discovery/loading entirely.
    bool enabled = true;

    // Directories whose scripts are loaded at startup and on reload, and that scripts import each other from.
    std::vector<std::filesystem::path> roots;
};

template<typename Visitor>
void reflect(Visitor& v, ScriptSettings& s)
{
    v.field("enabled", s.enabled, Doc{ "Load scripts at all; false skips discovery" });
    v.field("roots", s.roots, Doc{ "Directories scripts are loaded from at startup and reload" });
}

} // namespace oryx
