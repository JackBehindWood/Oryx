#pragma once

#include <fstream>

#include "Oryx.h"

namespace oryx::test
{

class TempDir
{
public:
    TempDir()
    {
        Random random;
        std::filesystem::path base = std::filesystem::temp_directory_path() / ("oryx-scripting-" + std::to_string(random.get_int()));
        std::filesystem::create_directories(base);
        m_path = std::filesystem::weakly_canonical(base);
    }

    ~TempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

    std::filesystem::path write(const std::string& relative, const std::string& content = "")
    {
        std::filesystem::path file = m_path / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream stream(file);
        stream << content;
        return file;
    }

private:
    std::filesystem::path m_path;
};

class FakeRuntime : public IScriptRuntime
{
public:
    FakeRuntime(std::string language, std::string extension, std::vector<std::string>& log, bool fail_start = false)
        : m_language(std::move(language))
        , m_extension(std::move(extension))
        , m_log(log)
        , m_fail_start(fail_start)
    {
    }

    std::string language() const override { return m_language; }
    std::vector<std::string> file_extensions() const override { return { m_extension }; }

    bool running() const override { return m_running; }

    void start() override
    {
        m_log.push_back(m_language + ":start");
        if (m_fail_start)
        {
            throw ScriptError("start failed");
        }
        m_running = true;
    }

    void stop() override
    {
        m_log.push_back(m_language + ":stop");
        m_running = false;
    }

    void load(const ScriptSource& source) override
    {
        m_log.push_back(m_language + ":load:" + label(source));
        if (source.target.find("broken") != std::string::npos)
        {
            throw ScriptError("broken script", "traceback text");
        }
    }

    void reload(const ScriptSource& source) override { m_log.push_back(m_language + ":reload:" + label(source)); }
    void unload() override { m_log.push_back(m_language + ":unload"); }

private:
    static std::string label(const ScriptSource& source)
    {
        return source.kind == ScriptSourceKind::Module ? source.target : std::filesystem::path(source.target).filename().string();
    }

    std::string m_language;
    std::string m_extension;
    std::vector<std::string>& m_log;
    bool m_fail_start;
    bool m_running = false;
};

// Owns fake runtimes; declare it before any ScopedScriptingShutdown so the fakes outlive the shutdown hooks that stop them.
class FakeRuntimeSet
{
public:
    void add(std::string language, std::string extension, std::vector<std::string>& log, bool fail_start = false)
    {
        m_runtimes.push_back(create_unique<FakeRuntime>(std::move(language), std::move(extension), log, fail_start));
    }

    [[nodiscard]] std::vector<IScriptRuntime*> pointers() const
    {
        std::vector<IScriptRuntime*> result;
        for (const UniquePtr<FakeRuntime>& runtime : m_runtimes)
        {
            result.push_back(runtime.get());
        }
        return result;
    }

private:
    std::vector<UniquePtr<FakeRuntime>> m_runtimes;
};

// Runs oryx::shutdown() on scope exit so runtimes started by a layer stop before the test's fixtures die.
class ScopedScriptingShutdown
{
public:
    ScopedScriptingShutdown() = default;
    ~ScopedScriptingShutdown() { shutdown(); }

    ScopedScriptingShutdown(const ScopedScriptingShutdown&) = delete;
    ScopedScriptingShutdown& operator=(const ScopedScriptingShutdown&) = delete;
};

} // namespace oryx::test
