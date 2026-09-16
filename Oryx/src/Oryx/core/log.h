#pragma once

#include <memory>

#include <spdlog/spdlog.h>

namespace oryx {

class Log {
public:
    static void Init();

    static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
    static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_ClientLogger;
};

} // namespace oryx

// Core: used within the Oryx engine itself.
#define ORYX_CORE_TRACE(...)    ::oryx::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define ORYX_CORE_INFO(...)     ::oryx::Log::GetCoreLogger()->info(__VA_ARGS__)
#define ORYX_CORE_WARN(...)     ::oryx::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define ORYX_CORE_ERROR(...)    ::oryx::Log::GetCoreLogger()->error(__VA_ARGS__)
#define ORYX_CORE_CRITICAL(...) ::oryx::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client: used by applications built on Oryx (e.g. Oasis).
#define ORYX_TRACE(...)         ::oryx::Log::GetClientLogger()->trace(__VA_ARGS__)
#define ORYX_INFO(...)          ::oryx::Log::GetClientLogger()->info(__VA_ARGS__)
#define ORYX_WARN(...)          ::oryx::Log::GetClientLogger()->warn(__VA_ARGS__)
#define ORYX_ERROR(...)         ::oryx::Log::GetClientLogger()->error(__VA_ARGS__)
#define ORYX_CRITICAL(...)      ::oryx::Log::GetClientLogger()->critical(__VA_ARGS__)
