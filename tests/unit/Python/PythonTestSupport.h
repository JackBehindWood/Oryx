#pragma once

#include "unit/Scripting/ScriptingTestSupport.h"

#ifdef OX_ENABLE_PYTHON

namespace oryx::test
{

inline ScriptSource file_source(const std::filesystem::path& file)
{
    return ScriptSource{ ScriptSourceKind::File, file.string(), "python" };
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

private:
    UniquePtr<IScriptRuntime> m_runtime;
};

} // namespace oryx::test

#endif
