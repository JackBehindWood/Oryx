#pragma once

#include <spdlog/sinks/ostream_sink.h>

#include "Oryx.h"

namespace oryx::test
{

// Swaps a logger for one writing "<level>|<message>" lines into a string; restores it on destruction.
class LogCapture
{
public:
    explicit LogCapture(std::shared_ptr<spdlog::logger>& target)
        : m_target(target)
        , m_previous(target)
    {
        std::shared_ptr<spdlog::sinks::ostream_sink_mt> sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(m_stream, true);
        sink->set_pattern("%l|%v");
        std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>("capture", sink);
        logger->set_level(spdlog::level::trace);
        m_target = logger;
    }

    ~LogCapture() { m_target = m_previous; }

    LogCapture(const LogCapture&) = delete;
    LogCapture& operator=(const LogCapture&) = delete;

    [[nodiscard]] std::vector<std::string> lines() const
    {
        std::vector<std::string> result;
        std::istringstream input(m_stream.str());
        for (std::string line; std::getline(input, line);)
        {
            result.push_back(line);
        }
        return result;
    }

private:
    std::ostringstream m_stream;
    std::shared_ptr<spdlog::logger>& m_target;
    std::shared_ptr<spdlog::logger> m_previous;
};

class ClientLogCapture : public LogCapture
{
public:
    ClientLogCapture()
        : LogCapture(Log::get_client_logger())
    {
    }
};

class CoreLogCapture : public LogCapture
{
public:
    CoreLogCapture()
        : LogCapture(Log::get_core_logger())
    {
    }
};

// Makes is_initialised() false for the scope so init guards can be exercised.
class UninitialisedScope
{
public:
    UninitialisedScope() { g_initialised = false; }
    ~UninitialisedScope() { g_initialised = true; }

    UninitialisedScope(const UninitialisedScope&) = delete;
    UninitialisedScope& operator=(const UninitialisedScope&) = delete;
};

} // namespace oryx::test
