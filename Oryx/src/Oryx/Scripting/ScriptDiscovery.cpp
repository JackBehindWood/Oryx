#include "oxpch.h"
#include "Oryx/Scripting/ScriptDiscovery.h"

namespace oryx
{

namespace
{

constexpr std::string_view kScriptFlag = "--script";
constexpr std::string_view kModuleFlag = "--module";
constexpr const char* kSearchPathVariable = "ORYX_SCRIPT_PATH";
constexpr char kPathSeparator = std::filesystem::path::preferred_separator == '/' ? ':' : ';';

void collect_flag_values(const ApplicationCommandLineArgs& args, std::string_view flag, std::vector<std::string>& out)
{
    for (int32_t i = 1; i < args.count; ++i)
    {
        std::string_view arg = args[i];
        if (arg == flag)
        {
            if (i + 1 < args.count)
            {
                out.emplace_back(args[++i]);
            }
        }
        else if (arg.size() > flag.size() && arg.substr(0, flag.size()) == flag && arg[flag.size()] == '=')
        {
            out.emplace_back(arg.substr(flag.size() + 1));
        }
    }
}

std::string extension_of(std::string_view pattern)
{
    size_t dot = pattern.rfind('.');
    return dot == std::string_view::npos ? "" : std::string(pattern.substr(dot));
}

std::string language_for_file_name(const std::string& file_name, const std::vector<ScriptFilePattern>& patterns)
{
    for (const ScriptFilePattern& entry : patterns)
    {
        if (matches_pattern(file_name, entry.pattern))
        {
            return entry.language;
        }
    }
    return "";
}

std::string language_for_extension(const std::string& extension, const std::vector<ScriptFilePattern>& patterns)
{
    if (extension.empty())
    {
        return "";
    }
    for (const ScriptFilePattern& entry : patterns)
    {
        if (extension_of(entry.pattern) == extension)
        {
            return entry.language;
        }
    }
    return "";
}

std::filesystem::path resolve_path(const std::filesystem::path& root, const std::string& entry)
{
    std::filesystem::path path(entry);
    if (path.is_relative())
    {
        path = root / path;
    }

    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    return error ? path.lexically_normal() : canonical;
}

bool is_skipped_directory(const std::filesystem::path& directory)
{
    std::string name = directory.filename().string();
    if (name == ".git" || name == ".venv" || name == "venv" || name == "build" || name.starts_with("bin"))
    {
        return true;
    }

    std::error_code error;
    return std::filesystem::exists(directory / "pyvenv.cfg", error);
}

std::vector<std::filesystem::path> scan_directory(const std::filesystem::path& directory,
                                                  const std::vector<ScriptFilePattern>& patterns)
{
    std::vector<std::filesystem::path> found;
    if (patterns.empty())
    {
        return found;
    }

    std::error_code error;
    std::filesystem::recursive_directory_iterator end;
    std::filesystem::recursive_directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, error);
    for (; !error && it != end; it.increment(error))
    {
        std::error_code entry_error;
        if (it->is_directory(entry_error))
        {
            if (is_skipped_directory(it->path()))
            {
                it.disable_recursion_pending();
            }
        }
        else if (it->is_regular_file(entry_error) && !language_for_file_name(it->path().filename().string(), patterns).empty())
        {
            found.push_back(it->path());
        }
    }

    std::sort(found.begin(), found.end());
    return found;
}

void add_source(std::vector<ScriptSource>& sources, std::vector<std::string>& seen, ScriptSourceKind kind, const std::string& target, const std::string& language)
{
    std::string key = (kind == ScriptSourceKind::Module ? "module:" : "file:") + target;
    if (std::find(seen.begin(), seen.end(), key) != seen.end())
    {
        return;
    }
    seen.push_back(key);
    sources.push_back(ScriptSource{ kind, target, language });
}

void add_file(std::vector<ScriptSource>& sources, std::vector<std::string>& seen, const std::filesystem::path& path, const std::string& language)
{
    add_source(sources, seen, ScriptSourceKind::File, path.string(), language);
}

} // namespace

ScriptDiscoveryOptions script_options(const ApplicationCommandLineArgs& args)
{
    ScriptDiscoveryOptions options;
    collect_flag_values(args, kScriptFlag, options.script_files);
    collect_flag_values(args, kModuleFlag, options.modules);

    const char* search_path = std::getenv(kSearchPathVariable);
    if (search_path != nullptr)
    {
        options.search_paths = split_search_path(search_path);
    }

    std::error_code error;
    options.root = std::filesystem::current_path(error);
    return options;
}

std::vector<std::string> split_search_path(std::string_view search_path)
{
    std::vector<std::string> entries;
    size_t start = 0;
    while (start <= search_path.size())
    {
        size_t end = search_path.find(kPathSeparator, start);
        if (end == std::string_view::npos)
        {
            end = search_path.size();
        }
        if (end > start)
        {
            entries.emplace_back(search_path.substr(start, end - start));
        }
        start = end + 1;
    }
    return entries;
}

bool matches_pattern(std::string_view file_name, std::string_view pattern)
{
    constexpr size_t kNoStar = std::string_view::npos;
    size_t name_index = 0;
    size_t pattern_index = 0;
    size_t star_index = kNoStar;
    size_t star_match = 0;

    while (name_index < file_name.size())
    {
        if (pattern_index < pattern.size() && pattern[pattern_index] == '*')
        {
            star_index = pattern_index++;
            star_match = name_index;
        }
        else if (pattern_index < pattern.size() && pattern[pattern_index] == file_name[name_index])
        {
            ++pattern_index;
            ++name_index;
        }
        else if (star_index != kNoStar)
        {
            pattern_index = star_index + 1;
            name_index = ++star_match;
        }
        else
        {
            return false;
        }
    }

    while (pattern_index < pattern.size() && pattern[pattern_index] == '*')
    {
        ++pattern_index;
    }
    return pattern_index == pattern.size();
}

std::vector<ScriptSource> discover_scripts(const ScriptDiscoveryOptions& options, const std::vector<ScriptFilePattern>& patterns)
{
    std::vector<ScriptSource> sources;
    std::vector<std::string> seen;

    for (const std::string& file : options.script_files)
    {
        std::filesystem::path path = resolve_path(options.root, file);
        add_file(sources, seen, path, language_for_extension(path.extension().string(), patterns));
    }

    for (const std::string& module : options.modules)
    {
        add_source(sources, seen, ScriptSourceKind::Module, module, "");
    }

    for (const std::string& entry : options.search_paths)
    {
        std::filesystem::path path = resolve_path(options.root, entry);
        std::error_code error;
        if (std::filesystem::is_directory(path, error))
        {
            for (const std::filesystem::path& found : scan_directory(path, patterns))
            {
                add_file(sources, seen, resolve_path(options.root, found.string()), language_for_file_name(found.filename().string(), patterns));
            }
        }
        else
        {
            add_file(sources, seen, path, language_for_extension(path.extension().string(), patterns));
        }
    }

    if (!options.root.empty())
    {
        for (const std::filesystem::path& found : scan_directory(options.root, patterns))
        {
            add_file(sources, seen, resolve_path(options.root, found.string()), language_for_file_name(found.filename().string(), patterns));
        }
    }

    return sources;
}

} // namespace oryx
