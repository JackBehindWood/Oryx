#include "oxpch.h"
#include "Oryx/Scripting/ScriptDiscovery.h"

namespace oryx
{

namespace
{

constexpr std::string_view kScriptFlag = "--script";
constexpr std::string_view kModuleFlag = "--module";
constexpr std::string_view kRootFlag = "--script-root";

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
            else
            {
                OX_CORE_WARN("ScriptDiscovery: {} needs a value - ignoring it.", flag);
            }
        }
        else if (arg.size() > flag.size() && arg.substr(0, flag.size()) == flag && arg[flag.size()] == '=')
        {
            out.emplace_back(arg.substr(flag.size() + 1));
        }
    }
}

std::string language_for(const std::filesystem::path& file, const std::vector<ScriptFileExtension>& extensions)
{
    std::string extension = file.extension().string();
    if (extension.empty())
    {
        return "";
    }
    for (const ScriptFileExtension& entry : extensions)
    {
        if (entry.extension == extension)
        {
            return entry.language;
        }
    }
    return "";
}

std::filesystem::path resolve_path(const std::filesystem::path& base, const std::string& entry)
{
    std::filesystem::path path(entry);
    if (path.is_relative())
    {
        path = base / path;
    }

    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    return error ? path.lexically_normal() : canonical;
}

bool is_helper(const std::filesystem::path& path)
{
    std::string name = path.filename().string();
    return name.starts_with('_') || name.starts_with('.');
}

bool is_inside(const std::filesystem::path& path, const std::filesystem::path& directory)
{
    std::filesystem::path relative = path.lexically_relative(directory);
    return !relative.empty() && *relative.begin() != "..";
}

std::vector<std::filesystem::path> scripts_under(const std::filesystem::path& root, const std::vector<ScriptFileExtension>& extensions)
{
    std::vector<std::filesystem::path> found;
    std::error_code error;
    std::filesystem::recursive_directory_iterator end;
    std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, error);
    for (; !error && it != end; it.increment(error))
    {
        if (is_helper(it->path()))
        {
            it.disable_recursion_pending();
            continue;
        }

        std::error_code entry_error;
        if (it->is_regular_file(entry_error) && !language_for(it->path(), extensions).empty())
        {
            found.push_back(it->path());
        }
    }

    std::sort(found.begin(), found.end());
    return found;
}

void add_file(std::vector<ScriptSource>& sources, const std::filesystem::path& file, const std::string& language, const std::filesystem::path& root)
{
    std::string target = file.string();
    bool seen = std::any_of(sources.begin(), sources.end(), [&target](const ScriptSource& source) { return source.kind == ScriptSourceKind::File && source.target == target; });
    if (!seen)
    {
        sources.push_back(ScriptSource{ ScriptSourceKind::File, target, language, root.string() });
    }
}

} // namespace

ScriptDiscoveryOptions script_options(const ApplicationCommandLineArgs& args)
{
    ScriptDiscoveryOptions options;
    collect_flag_values(args, kScriptFlag, options.script_files);
    collect_flag_values(args, kModuleFlag, options.modules);
    collect_flag_values(args, kRootFlag, options.roots);

    std::error_code error;
    options.root = std::filesystem::current_path(error);
    return options;
}

std::vector<ScriptSource> discover_scripts(const ScriptDiscoveryOptions& options, const std::vector<ScriptFileExtension>& extensions)
{
    std::vector<std::filesystem::path> roots;
    for (const std::string& entry : options.roots)
    {
        std::filesystem::path root = resolve_path(options.root, entry);
        std::error_code error;
        if (!std::filesystem::is_directory(root, error))
        {
            OX_CORE_WARN("ScriptDiscovery: script root '{}' is not a directory - skipping it.", root.string());
            continue;
        }
        if (std::find(roots.begin(), roots.end(), root) == roots.end())
        {
            roots.push_back(root);
        }
    }

    std::vector<ScriptSource> sources;
    for (const std::string& entry : options.script_files)
    {
        std::filesystem::path file = resolve_path(options.root, entry);
        auto owner = std::find_if(roots.begin(), roots.end(), [&file](const std::filesystem::path& root) { return is_inside(file, root); });
        add_file(sources, file, language_for(file, extensions), owner == roots.end() ? file.parent_path() : *owner);
    }

    for (const std::string& module : options.modules)
    {
        bool seen = std::any_of(sources.begin(), sources.end(), [&module](const ScriptSource& source) { return source.kind == ScriptSourceKind::Module && source.target == module; });
        if (!seen)
        {
            sources.push_back(ScriptSource{ ScriptSourceKind::Module, module, "", "" });
        }
    }

    for (const std::filesystem::path& root : roots)
    {
        for (const std::filesystem::path& file : scripts_under(root, extensions))
        {
            add_file(sources, file, language_for(file, extensions), root);
        }
    }

    return sources;
}

} // namespace oryx
