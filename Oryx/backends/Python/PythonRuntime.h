#pragma once

#include "Oryx/Scripting/Interfaces/IScriptRuntime.h"

namespace oryx
{

class PythonRuntime : public IScriptRuntime
{
public:
    PythonRuntime() = default;
    ~PythonRuntime() override;

    PythonRuntime(const PythonRuntime&) = delete;
    PythonRuntime& operator=(const PythonRuntime&) = delete;

    std::string language() const override;
    std::vector<std::string> file_extensions() const override;

    bool running() const override { return m_running; }

    void start() override;
    void stop() override;

    void load(const ScriptSource& source) override;
    void reload(const ScriptSource& source) override;
    void unload() override;

private:
    void run_source(const ScriptSource& source, bool reload);
    void add_root(const std::string& root);

    bool m_running = false;
    std::vector<std::string> m_roots;
};

} // namespace oryx
