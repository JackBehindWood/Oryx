#include "oxpch.h"
#include "PythonRuntime.h"

#include "Bindings/BindOryx.h"
#include "Interop/PyScriptObject.h"
#include "PythonContext.h"
#include "PythonConfig.h"
#include "PythonHost.h"
#include "PythonLanguage.h"
#include "Support/PyUtil.h"

#include <pybind11/embed.h>

#include "Oryx/Scripting/Support/ScriptError.h"
#include "Oryx/Scripting/Registry/ScriptRegistry.h"
#include "Oryx/Scripting/Registry/ScriptRuntimeRegistry.h"

namespace py = pybind11;

namespace oryx
{

namespace
{

constexpr char kPathSeparator = std::filesystem::path::preferred_separator == '/' ? ':' : ';';

void set_config_string(PyConfig& config, wchar_t** field, const char* value)
{
    PyStatus status = PyConfig_SetBytesString(&config, field, value);
    if (PyStatus_Exception(status) != 0)
    {
        PyConfig_Clear(&config);
        throw ScriptError("PythonRuntime: could not configure the interpreter", status.err_msg ? status.err_msg : "");
    }
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

void extend_search_path()
{
    py::list search_path = py::module_::import("sys").attr("path");
    for (const std::string& site_packages : split_search_path(OX_PYTHON_SITE_PACKAGES))
    {
        search_path.append(site_packages);
    }
}

bool is_inside(const std::filesystem::path& path, const std::filesystem::path& directory)
{
    std::filesystem::path relative = path.lexically_relative(directory);
    return !relative.empty() && *relative.begin() != "..";
}

bool is_under_a_root(const py::handle& module, const std::vector<std::string>& roots)
{
    std::vector<std::string> locations;
    py::object file = py::getattr(module, "__file__", py::none());
    if (py::isinstance<py::str>(file))
    {
        locations.push_back(file.cast<std::string>());
    }
    else if (py::hasattr(module, "__path__"))
    {
        for (const py::handle& entry : py::list(module.attr("__path__")))
        {
            locations.push_back(py::str(entry));
        }
    }

    return std::any_of(locations.begin(), locations.end(), [&roots](const std::string& location)
    {
        return std::any_of(roots.begin(), roots.end(), [&location](const std::string& root) { return is_inside(location, root); });
    });
}

// Forgets every module a script root provided, and breaks the reference cycles the garbage collector cannot see through C++ owners.
void purge_modules(const std::vector<std::string>& roots)
{
    py::object modules = py::module_::import("sys").attr("modules");

    // Decide first: a namespace package's __path__ looks its parent up in sys.modules, so nothing may be removed while deciding.
    std::vector<std::pair<py::object, py::object>> doomed;
    for (const py::handle& item : py::list(modules.attr("items")()))
    {
        py::object module = py::reinterpret_borrow<py::object>(item[py::int_(1)]);
        if (is_under_a_root(module, roots))
        {
            doomed.emplace_back(py::reinterpret_borrow<py::object>(item[py::int_(0)]), module);
        }
    }

    for (const std::pair<py::object, py::object>& entry : doomed)
    {
        modules.attr("pop")(entry.first, py::none());
        if (py::hasattr(entry.second, "__dict__"))
        {
            entry.second.attr("__dict__").attr("clear")();
        }
    }
    py::module_::import("gc").attr("collect")();
    py::module_::import("importlib").attr("invalidate_caches")();
}

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
            throw ScriptError("PythonRuntime: cannot import '" + source.target + "': a script's name cannot contain a dot");
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
        throw ScriptError("PythonRuntime: script '" + source.target + "' is shadowed by the module '" + name + "'" + other + "; rename the script");
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

} // namespace

PythonRuntime::~PythonRuntime()
{
    stop();
}

std::string PythonRuntime::language() const
{
    return python::kLanguage;
}

std::vector<std::string> PythonRuntime::file_extensions() const
{
    return { ".py" };
}

void PythonRuntime::start()
{
    if (m_running)
    {
        return;
    }

    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    config.parse_argv = 0;
    config.install_signal_handlers = 0;
    config.write_bytecode = 0;
    if (!std::getenv("PYTHONHOME"))
    {
        set_config_string(config, &config.home, OX_PYTHON_HOME);
    }

    bool failed = false;
    std::string reason;
    try
    {
        python::register_oryx_module();
        py::initialize_interpreter(&config, 0, nullptr, false);
        python::mark_embedded_host();
        extend_search_path();
    }
    catch (const std::exception& error)
    {
        failed = true;
        reason = error.what();
    }

    if (failed)
    {
        // Finalise outside the catch: a py::error_already_set must not outlive the interpreter.
        if (Py_IsInitialized() != 0)
        {
            py::finalize_interpreter();
        }
        throw ScriptError("PythonRuntime: could not start the interpreter", reason);
    }
    m_running = true;
}

void PythonRuntime::stop()
{
    if (!m_running)
    {
        return;
    }

    unload();
    python::PythonContext::reset();
    if (python::live_script_objects() > 0)
    {
        OX_CORE_WARN("PythonRuntime: {} script object(s) are still alive at stop and will be leaked; destroy games and strategies before oryx::shutdown().", python::live_script_objects());
    }
    py::finalize_interpreter();
    m_running = false;
}

void PythonRuntime::run_source(const ScriptSource& source, bool reload)
{
    try
    {
        if (source.kind == ScriptSourceKind::Module)
        {
            import_module(source.target, reload);
            return;
        }

        if (source.root.empty())
        {
            throw ScriptError("PythonRuntime: the script '" + source.target + "' has no root to import it from");
        }
        add_root(source.root);
        import_file(source, reload);
    }
    catch (const py::error_already_set& error)
    {
        throw python::to_script_error(error, "PythonRuntime: could not " + std::string(reload ? "reload" : "load") + " '" + source.target + "'");
    }
}

void PythonRuntime::add_root(const std::string& root)
{
    if (std::find(m_roots.begin(), m_roots.end(), root) != m_roots.end())
    {
        return;
    }

    py::list search_path = py::module_::import("sys").attr("path");
    search_path.append(root);
    py::module_::import("importlib").attr("invalidate_caches")();
    m_roots.push_back(root);
}

void PythonRuntime::load(const ScriptSource& source)
{
    if (!m_running)
    {
        throw ScriptError("PythonRuntime::load() was called before start()");
    }
    run_source(source, false);
}

void PythonRuntime::reload(const ScriptSource& source)
{
    if (!m_running)
    {
        throw ScriptError("PythonRuntime::reload() was called before start()");
    }
    run_source(source, true);
}

void PythonRuntime::unload()
{
    unregister_scripted(python::kLanguage);
    if (!m_running)
    {
        return;
    }

    purge_modules(m_roots);

    py::list search_path = py::module_::import("sys").attr("path");
    for (const std::string& root : m_roots)
    {
        if (search_path.contains(root))
        {
            search_path.attr("remove")(root);
        }
    }
    m_roots.clear();
}

OX_REGISTER_SCRIPT_RUNTIME(PythonRuntime, "python")

} // namespace oryx
