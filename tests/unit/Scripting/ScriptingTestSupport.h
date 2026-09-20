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
    FakeRuntime(std::string language, std::string pattern, std::vector<std::string>& log, bool fail_start = false)
        : m_language(std::move(language))
        , m_pattern(std::move(pattern))
        , m_log(log)
        , m_fail_start(fail_start)
    {
    }

    std::string language() const override { return m_language; }
    std::vector<std::string> file_patterns() const override { return { m_pattern }; }

    void start() override
    {
        m_log.push_back(m_language + ":start");
        if (m_fail_start)
        {
            throw ScriptError("start failed");
        }
    }

    void stop() override { m_log.push_back(m_language + ":stop"); }

    void load(const ScriptSource& source) override
    {
        m_log.push_back(m_language + ":load:" + label(source));
        if (source.target.find("broken") != std::string::npos)
        {
            throw ScriptError("broken script", "traceback text");
        }
    }

    void reload(const ScriptSource& source) override { m_log.push_back(m_language + ":reload:" + label(source)); }

private:
    static std::string label(const ScriptSource& source)
    {
        return source.kind == ScriptSourceKind::Module ? source.target : std::filesystem::path(source.target).filename().string();
    }

    std::string m_language;
    std::string m_pattern;
    std::vector<std::string>& m_log;
    bool m_fail_start;
};

} // namespace oryx::test
