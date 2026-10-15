#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `resources:` section: one folder per application holding source assets and, under compiled/, their derived counterparts.
struct ResourceSettings
{
    // A relative root is relative to the settings file.
    std::filesystem::path root = "resources";
    bool compiled_enabled = true;
};

template<typename Visitor>
void reflect(Visitor& v, ResourceSettings& s)
{
    v.field("root", s.root, Doc{ "Resource folder; a relative path is relative to the settings file" });
    v.field("compiled_enabled", s.compiled_enabled, Doc{ "Use derived assets under compiled/" });
}

[[nodiscard]] std::filesystem::path compiled_directory(const ResourceSettings& settings);

} // namespace oryx
