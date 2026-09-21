#pragma once

#include "Oryx/Core/Application.h"
#include "Oryx/Core/Base.h"
#include "Oryx/Scripting/Support/ScriptSource.h"

namespace oryx
{

struct ScriptFileExtension
{
    std::string language;
    std::string extension;
};

struct ScriptDiscoveryOptions
{
    std::vector<std::string> script_files;
    std::vector<std::string> modules;
    std::vector<std::string> roots;
    // Relative paths above are taken relative to this directory.
    std::filesystem::path root;
};

// Reads --script, --module and --script-root (each as `--flag value` or `--flag=value`); a flag without a value is warned about.
[[nodiscard]] ScriptDiscoveryOptions script_options(const ApplicationCommandLineArgs& args);

// The --script files, then the --module names, then every script file under each root in path order.
// A file or directory whose name starts with `_` or `.` is skipped (helpers are imported, never loaded on their own).
[[nodiscard]] std::vector<ScriptSource> discover_scripts(const ScriptDiscoveryOptions& options, const std::vector<ScriptFileExtension>& extensions);

} // namespace oryx
