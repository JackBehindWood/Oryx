#pragma once

#include "Oryx/Core/Base.h"

namespace spdlog
{
class logger;
}

namespace oryx
{

inline constexpr uint32_t k_log_capacity = 2048;
inline constexpr uint32_t k_log_text_capacity = 240;

enum class LogSource : uint8_t
{
    Core,
    App
};

// `level` is the spdlog level number (trace 0 ... critical 5).
struct LogLine
{
    uint64_t sequence = 0;
    uint16_t length = 0;
    uint8_t level = 0;
    LogSource source = LogSource::Core;
    char text[k_log_text_capacity] = {};
};

// The last k_log_capacity lines of both loggers in a fixed ring, written from any thread and read through snapshots. Without OX_ENABLE_LOG_CAPTURE every call does nothing and available() is false.
namespace log_capture
{

[[nodiscard]] inline bool available()
{
#if defined(OX_ENABLE_LOG_CAPTURE)
    return true;
#else
    return false;
#endif
}
// A line longer than k_log_text_capacity is cut at a character boundary and ends in an ellipsis; a full ring overwrites its oldest line and counts it as dropped.
void record(uint8_t level, LogSource source, std::string_view text);
// Lines recorded since the start (or the last clear): sequences run 1..sequence().
[[nodiscard]] uint64_t sequence();
[[nodiscard]] uint64_t dropped();
// Copies the held lines, oldest first, into `out` (at most `capacity`, the newest when more are held); returns how many.
uint32_t snapshot(LogLine* out, uint32_t capacity);
void clear();
// Adds the capture as a sink of `logger`; a no-op when not available.
void attach(spdlog::logger& logger, LogSource source);

} // namespace log_capture

} // namespace oryx
