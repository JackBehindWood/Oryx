#include "oxpch.h"
#include "Oryx/Core/LogCapture.h"

#ifdef OX_ENABLE_LOG_CAPTURE
#include <spdlog/sinks/base_sink.h>
#endif

namespace oryx::log_capture
{

#ifdef OX_ENABLE_LOG_CAPTURE

namespace
{

constexpr char k_ellipsis[] = "\xE2\x80\xA6";
constexpr uint32_t k_ellipsis_size = 3;

struct Ring
{
    std::mutex lock;
    LogLine lines[k_log_capacity];
    uint32_t start = 0;
    uint32_t count = 0;
    uint64_t next_sequence = 1;
    uint64_t dropped = 0;
};

Ring& ring()
{
    static Ring instance;
    return instance;
}

uint16_t fit(std::string_view text, char* out)
{
    if (text.size() <= k_log_text_capacity)
    {
        std::memcpy(out, text.data(), text.size());
        return static_cast<uint16_t>(text.size());
    }
    size_t cut = k_log_text_capacity - k_ellipsis_size;
    while (cut > 0 && (static_cast<uint8_t>(text[cut]) & 0xC0) == 0x80)
    {
        --cut;
    }
    std::memcpy(out, text.data(), cut);
    std::memcpy(out + cut, k_ellipsis, k_ellipsis_size);
    return static_cast<uint16_t>(cut + k_ellipsis_size);
}

class CaptureSink final : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
public:
    explicit CaptureSink(LogSource source)
        : m_source(source)
    {
    }

protected:
    void sink_it_(const spdlog::details::log_msg& message) override
    {
        record(static_cast<uint8_t>(message.level), m_source, std::string_view(message.payload.data(), message.payload.size()));
    }

    void flush_() override {}

private:
    LogSource m_source;
};

} // namespace

void record(uint8_t level, LogSource source, std::string_view text)
{
    Ring& r = ring();
    std::lock_guard<std::mutex> guard(r.lock);
    uint32_t slot = 0;
    if (r.count == k_log_capacity)
    {
        slot = r.start;
        r.start = (r.start + 1) % k_log_capacity;
        ++r.dropped;
    }
    else
    {
        slot = (r.start + r.count) % k_log_capacity;
        ++r.count;
    }
    LogLine& line = r.lines[slot];
    line.sequence = r.next_sequence++;
    line.level = level;
    line.source = source;
    line.length = fit(text, line.text);
}

uint64_t sequence()
{
    Ring& r = ring();
    std::lock_guard<std::mutex> guard(r.lock);
    return r.next_sequence - 1;
}

uint64_t dropped()
{
    Ring& r = ring();
    std::lock_guard<std::mutex> guard(r.lock);
    return r.dropped;
}

uint32_t snapshot(LogLine* out, uint32_t capacity)
{
    Ring& r = ring();
    std::lock_guard<std::mutex> guard(r.lock);
    const uint32_t count = capacity < r.count ? capacity : r.count;
    const uint32_t skip = r.count - count;
    for (uint32_t i = 0; i < count; ++i)
    {
        out[i] = r.lines[(r.start + skip + i) % k_log_capacity];
    }
    return count;
}

void clear()
{
    Ring& r = ring();
    std::lock_guard<std::mutex> guard(r.lock);
    r.start = 0;
    r.count = 0;
    r.next_sequence = 1;
    r.dropped = 0;
}

void attach(spdlog::logger& logger, LogSource source)
{
    logger.sinks().push_back(std::make_shared<CaptureSink>(source));
}

#else

void record(uint8_t, LogSource, std::string_view) {}

uint64_t sequence()
{
    return 0;
}

uint64_t dropped()
{
    return 0;
}

uint32_t snapshot(LogLine*, uint32_t)
{
    return 0;
}

void clear() {}

void attach(spdlog::logger&, LogSource) {}

#endif

} // namespace oryx::log_capture
