#pragma once

#include <memory>

#include <spdlog/spdlog.h>

namespace oryx 
{

class Log 
{
public:
    static void init();

    static std::shared_ptr<spdlog::logger>& get_core_logger() { return s_core_logger; }
    static std::shared_ptr<spdlog::logger>& get_client_logger() { return s_client_logger; }

private:
    static std::shared_ptr<spdlog::logger> s_core_logger;
    static std::shared_ptr<spdlog::logger> s_client_logger;
};

} // namespace oryx

// Core: used within the Oryx engine itself.
#define ORYX_CORE_TRACE(...)    ::oryx::Log::get_core_logger()->trace(__VA_ARGS__)
#define ORYX_CORE_INFO(...)     ::oryx::Log::get_core_logger()->info(__VA_ARGS__)
#define ORYX_CORE_WARN(...)     ::oryx::Log::get_core_logger()->warn(__VA_ARGS__)
#define ORYX_CORE_ERROR(...)    ::oryx::Log::get_core_logger()->error(__VA_ARGS__)
#define ORYX_CORE_CRITICAL(...) ::oryx::Log::get_core_logger()->critical(__VA_ARGS__)

// Client: used by applications built on Oryx (e.g. Oasis).
#define ORYX_TRACE(...)         ::oryx::Log::get_client_logger()->trace(__VA_ARGS__)
#define ORYX_INFO(...)          ::oryx::Log::get_client_logger()->info(__VA_ARGS__)
#define ORYX_WARN(...)          ::oryx::Log::get_client_logger()->warn(__VA_ARGS__)
#define ORYX_ERROR(...)         ::oryx::Log::get_client_logger()->error(__VA_ARGS__)
#define ORYX_CRITICAL(...)      ::oryx::Log::get_client_logger()->critical(__VA_ARGS__)
