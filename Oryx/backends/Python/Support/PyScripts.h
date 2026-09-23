#pragma once

#include "Oryx/Scripting/Support/ScriptSource.h"

namespace oryx::python
{

// Appends `root` to sys.path unless it is there already.
void add_search_root(const std::string& root);

// Imports (or reloads) a script file or module; a file's root must already be on sys.path. Throws py::error_already_set or ScriptError.
void import_script(const ScriptSource& source, bool reload);

// Reads `settings_file`, else the nearest oryx.yaml above the working directory (none: nothing), and imports its scripting roots.
void load_configured_scripts(const std::filesystem::path& settings_file);

} // namespace oryx::python
