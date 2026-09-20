#pragma once

#include "Oryx/Core/Application.h"
#include "Oryx/Core/Base.h"
#include "Oryx/Scripting/ScriptSource.h"

namespace oryx
{

struct ScriptFilePattern
{
    std::string language;
    std::string pattern;
};

struct ScriptDiscoveryOptions
{
    std::vector<std::string> script_files;
    std::vector<std::string> modules;
    std::vector<std::string> search_paths;
    std::filesystem::path root;
};

[[nodiscard]] ScriptDiscoveryOptions script_options(const ApplicationCommandLineArgs& args);

[[nodiscard]] std::vector<std::string> split_search_path(std::string_view search_path);

[[nodiscard]] bool matches_pattern(std::string_view file_name, std::string_view pattern);

[[nodiscard]] std::vector<ScriptSource> discover_scripts(const ScriptDiscoveryOptions& options,
                                                         const std::vector<ScriptFilePattern>& patterns);

} // namespace oryx
