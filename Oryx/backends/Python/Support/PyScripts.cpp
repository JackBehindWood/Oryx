#include "oxpch.h"
#include "Support/PyScripts.h"

#include <pybind11/pybind11.h>

#include "PythonLanguage.h"
#include "Support/PyUtil.h"
#include "Oryx/Core/Settings.h"
#include "Oryx/Scripting/ScriptDiscovery.h"
#include "Oryx/Scripting/ScriptSettings.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

// The dotted name a script is imported by: its path below its root, without the extension.
std::string module_name_of(const ScriptSource& source)
{
    std::filesystem::path relative = std::filesystem::path(source.target).lexically_relative(source.root);
    relative.replace_extension();

    std::string name;
    for (const std::filesystem::path& part : relative)
    {
        if (part.string().find('.') != std::string::npos)
        {
            throw ScriptError("cannot import '" + source.target + "': a script's name cannot contain a dot");
        }
        name += (name.empty() ? "" : ".") + part.string();
    }
    return name;
}

void import_file(const ScriptSource& source, bool reload)
{
    std::string name = module_name_of(source);
    py::object modules = py::module_::import("sys").attr("modules");
    py::module_ importlib = py::module_::import("importlib");
    py::object module = reload && modules.contains(name.c_str()) ? importlib.attr("reload")(modules[name.c_str()]) : importlib.attr("import_module")(name);

    py::object file = py::getattr(module, "__file__", py::none());
    std::error_code error;
    if (!py::isinstance<py::str>(file) || !std::filesystem::equivalent(file.cast<std::string>(), source.target, error))
    {
        std::string other = py::isinstance<py::str>(file) ? " (" + file.cast<std::string>() + ")" : "";
        throw ScriptError("script '" + source.target + "' is shadowed by the module '" + name + "'" + other + "; rename the script");
    }
}

void import_module(const std::string& target, bool reload)
{
    py::object modules = py::module_::import("sys").attr("modules");
    if (reload && modules.contains(target.c_str()))
    {
        py::module_::import("importlib").attr("reload")(modules[target.c_str()]);
        return;
    }
    py::module_::import(target.c_str());
}

std::filesystem::path nearest_settings_file()
{
    std::error_code error;
    for (std::filesystem::path directory = std::filesystem::current_path(error); !directory.empty(); directory = directory.parent_path())
    {
        std::filesystem::path candidate = directory / "oryx.yaml";
        if (std::filesystem::is_regular_file(candidate, error))
        {
            return candidate;
        }
        if (directory == directory.parent_path())
        {
            break;
        }
    }
    return {};
}

} // namespace

void add_search_root(const std::string& root)
{
    py::list search_path = py::module_::import("sys").attr("path");
    if (search_path.contains(root))
    {
        return;
    }
    search_path.append(root);
    py::module_::import("importlib").attr("invalidate_caches")();
}

void import_script(const ScriptSource& source, bool reload)
{
    if (source.kind == ScriptSourceKind::Module)
    {
        import_module(source.target, reload);
        return;
    }
    if (source.root.empty())
    {
        throw ScriptError("the script '" + source.target + "' has no root to import it from");
    }
    import_file(source, reload);
}

void load_configured_scripts(const std::filesystem::path& settings_file)
{
    std::filesystem::path file = settings_file.empty() ? nearest_settings_file() : settings_file;
    if (file.empty())
    {
        return;
    }

    std::string flag = "--settings=" + file.string();
    std::array<char*, 2> argv = { const_cast<char*>(kModuleName), flag.data() };
    load_settings(ApplicationCommandLineArgs{ static_cast<int32_t>(argv.size()), argv.data() });

    const ScriptSettings& scripting = settings_of<ScriptSettings>();
    if (!scripting.enabled)
    {
        return;
    }

    ScriptDiscoveryOptions options;
    options.root = file.parent_path();
    for (const std::filesystem::path& root : scripting.roots)
    {
        options.roots.push_back(root.string());
    }

    for (const ScriptSource& source : discover_scripts(options, { ScriptFileExtension{ kLanguage, ".py" } }))
    {
        try
        {
            add_search_root(source.root);
            import_script(source, false);
        }
        catch (const py::error_already_set& error)
        {
            rethrow_if_interpreter_control(error);
            throw to_script_error(error, "could not load '" + source.target + "'");
        }
    }
}

} // namespace oryx::python
