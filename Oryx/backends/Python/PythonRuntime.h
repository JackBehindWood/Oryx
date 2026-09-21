#pragma once

#include "Oryx/Scripting/IScriptRuntime.h"

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
    std::vector<std::string> file_patterns() const override;

    void start() override;
    void stop() override;

    void load(const ScriptSource& source) override;
    void reload(const ScriptSource& source) override;
    void unload() override;

private:
    bool m_running = false;
};

} // namespace oryx
