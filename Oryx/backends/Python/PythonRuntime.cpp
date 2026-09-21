#include "oxpch.h"
#include "PythonRuntime.h"

#include "BindOryx.h"
#include "PythonSupport.h"

#include <pybind11/embed.h>

#include "Oryx/Scripting/ScriptError.h"
#include "Oryx/Scripting/ScriptRegistry.h"
#include "Oryx/Scripting/ScriptRuntimeRegistry.h"

namespace py = pybind11;

namespace oryx
{

namespace
{

constexpr const char* kScriptModulePrefix = "oryx_script_";

void set_config_string(PyConfig& config, wchar_t** field, const char* value)
{
    PyStatus status = PyConfig_SetBytesString(&config, field, value);
    if (PyStatus_Exception(status) != 0)
    {
        PyConfig_Clear(&config);
        throw ScriptError("PythonRuntime: could not configure the interpreter", status.err_msg ? status.err_msg : "");
    }
}

std::string script_module_name(const std::filesystem::path& file)
{
    std::string stem = file.filename().string();
    stem = stem.substr(0, stem.find('.'));
    for (char& c : stem)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)))
        {
            c = '_';
        }
    }
    return kScriptModulePrefix + stem;
}

void load_file(const std::string& target)
{
    std::filesystem::path file = std::filesystem::absolute(target);
    if (!std::filesystem::is_regular_file(file))
    {
        throw ScriptError("PythonRuntime: script file not found: " + file.string());
    }

    std::string name = script_module_name(file);
    py::module_ util = py::module_::import("importlib.util");
    py::object spec = util.attr("spec_from_file_location")(name, file.string());
    py::object module = util.attr("module_from_spec")(spec);

    py::object modules = py::module_::import("sys").attr("modules");
    modules[name.c_str()] = module;
    try
    {
        spec.attr("loader").attr("exec_module")(module);
    }
    catch (const py::error_already_set&)
    {
        modules.attr("pop")(name, py::none());
        throw;
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

void run_source(const ScriptSource& source, bool reload)
{
    try
    {
        if (source.kind == ScriptSourceKind::Module)
        {
            import_module(source.target, reload);
        }
        else
        {
            load_file(source.target);
        }
    }
    catch (const py::error_already_set& error)
    {
        throw python::to_script_error(error, "PythonRuntime: could not " + std::string(reload ? "reload" : "load") + " '" + source.target + "'");
    }
}

} // namespace

PythonRuntime::~PythonRuntime()
{
    stop();
}

std::string PythonRuntime::language() const
{
    return "python";
}

std::vector<std::string> PythonRuntime::file_patterns() const
{
    return { "*.oryx.py" };
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

    try
    {
        python::register_oryx_module();
        py::initialize_interpreter(&config, 0, nullptr, false);
        py::module_::import("sys").attr("path").attr("insert")(0, OX_PYTHON_PACKAGE_DIR);
    }
    catch (const std::runtime_error& error)
    {
        throw ScriptError("PythonRuntime: could not start the interpreter", error.what());
    }
    m_running = true;
}

void PythonRuntime::stop()
{
    if (!m_running)
    {
        return;
    }

    unregister_scripted("python");
    py::finalize_interpreter();
    m_running = false;
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

OX_REGISTER_SCRIPT_RUNTIME(PythonRuntime, "python")

} // namespace oryx
