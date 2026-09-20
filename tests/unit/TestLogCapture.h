#pragma once

#include <spdlog/sinks/ostream_sink.h>

#include "Oryx.h"

namespace oryx::test
{

// Swaps the client logger for one writing "<level>|<message>" lines into a string; restores it on destruction.
class ClientLogCapture
{
public:
    ClientLogCapture()
        : m_previous(Log::get_client_logger())
    {
        std::shared_ptr<spdlog::sinks::ostream_sink_mt> sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(m_stream, true);
        sink->set_pattern("%l|%v");
        std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>("capture", sink);
        logger->set_level(spdlog::level::trace);
        Log::get_client_logger() = logger;
    }

    ~ClientLogCapture() { Log::get_client_logger() = m_previous; }

    ClientLogCapture(const ClientLogCapture&) = delete;
    ClientLogCapture& operator=(const ClientLogCapture&) = delete;

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
    std::shared_ptr<spdlog::logger> m_previous;
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
