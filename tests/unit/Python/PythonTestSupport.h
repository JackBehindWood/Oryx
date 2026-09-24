#pragma once

#include "doctest.h"

#include "unit/Scripting/ScriptingTestSupport.h"

#ifdef OX_ENABLE_PYTHON

namespace oryx::python
{

// Defined in the private Python backend (Oryx/backends/Python), which Tests does not include.
[[nodiscard]] int64_t live_script_objects();

} // namespace oryx::python

namespace oryx::test
{

inline std::filesystem::path extension_path()
{
    return std::filesystem::path(OX_BUILD_OUTPUT_DIR) / "OryxPython";
}

// A sanitized oryx.so needs the ASan runtime preloaded, which a plain `python` subprocess never has.
inline bool can_run_python_extension()
{
    if (!std::filesystem::exists(extension_path()))
    {
        MESSAGE("skipping: research-host extension not built at ", extension_path().string());
        return false;
    }
#if defined(__has_feature)
    #if __has_feature(address_sanitizer)
    MESSAGE("skipping: --sanitize build - the sanitizer runtime isn't preloaded into the python subprocess");
    return false;
    #endif
#endif
#if defined(__SANITIZE_ADDRESS__)
    MESSAGE("skipping: --sanitize build - the sanitizer runtime isn't preloaded into the python subprocess");
    return false;
#endif
    return true;
}

// `python` from PATH: under `uv run` (or forge's own PATH prepend) that is the venv interpreter the research-host .pth lives in.
inline int run_python(const std::string& code, const std::string& environment = "")
{
    std::string command = environment + "python -c \"" + code + "\"";
    return std::system(command.c_str());
}

inline ScriptSource file_source(const std::filesystem::path& file)
{
    return ScriptSource{ ScriptSourceKind::File, file.string(), "python", file.parent_path().string() };
}

inline ScriptSource module_source(const std::string& name)
{
    return ScriptSource{ ScriptSourceKind::Module, name, "python" };
}

// Python source defining mark(text), which appends to a file the test reads back.
inline std::string marker_prelude(const std::filesystem::path& marker)
{
    return "def mark(text):\n    with open(\"" + marker.generic_string() + "\", \"a\") as f:\n        f.write(text)\n";
}

inline std::string append_marker(const std::filesystem::path& marker, const std::string& text)
{
    return marker_prelude(marker) + "mark(\"" + text + "\")\n";
}

inline std::string read_file(const std::filesystem::path& file)
{
    std::ifstream stream(file);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

inline std::filesystem::path repo_file(const std::string& relative)
{
    for (std::filesystem::path dir = std::filesystem::current_path(); dir.has_parent_path() && dir != dir.parent_path(); dir = dir.parent_path())
    {
        if (std::filesystem::exists(dir / relative))
        {
            return dir / relative;
        }
    }
    throw std::runtime_error("cannot find " + relative + " above the working directory");
}

inline UniquePtr<IScriptRuntime> python_runtime()
{
    return ScriptRuntimeRegistry::create("python");
}

// Starts a Python runtime and stops it on scope exit, even when load() throws.
class RunningPython
{
public:
    RunningPython()
        : m_runtime(python_runtime())
    {
        m_runtime->start();
    }

    ~RunningPython() { m_runtime->stop(); }

    RunningPython(const RunningPython&) = delete;
    RunningPython& operator=(const RunningPython&) = delete;

    void load(const std::filesystem::path& file) { m_runtime->load(file_source(file)); }
    void reload(const std::filesystem::path& file) { m_runtime->reload(file_source(file)); }

private:
    UniquePtr<IScriptRuntime> m_runtime;
};

// Runs `body` after `import oryx` with mark() available and returns what it marked.
inline std::string run_oryx_script(const std::string& body)
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("script.py", marker_prelude(marker) + "import oryx\n" + body);

    RunningPython python;
    python.load(script);
    return read_file(marker);
}

} // namespace oryx::test

#endif
