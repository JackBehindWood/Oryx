#include "log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace oryx {

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

void Log::Init() {
    spdlog::set_pattern("%^[%T] %n: %v%$");

    s_CoreLogger = spdlog::stdout_color_mt("ORYX");
    s_ClientLogger = spdlog::stdout_color_mt("APP");

#ifdef ORYX_DEBUG
    s_CoreLogger->set_level(spdlog::level::trace);
    s_ClientLogger->set_level(spdlog::level::trace);
#else
    s_CoreLogger->set_level(spdlog::level::info);
    s_ClientLogger->set_level(spdlog::level::info);
#endif
}

} // namespace oryx
