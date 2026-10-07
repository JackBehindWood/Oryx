#include "doctest.h"

#include "Oryx.h"

#include <chrono>

using namespace oryx;

namespace
{

struct BenchContext : ImContext
{
};

using Clock = std::chrono::steady_clock;

double milliseconds_per_frame(uint32_t items, uint32_t frames)
{
    BenchContext context;
    ImInput input;
    input.surface_size = { 1920.0f, 1080.0f };
    const Rect rect = { { 0.0f, 0.0f }, { 10.0f, 10.0f } };
    for (uint32_t warm = 0; warm < 2; ++warm)
    {
        context.begin_frame(input);
        for (uint32_t index = 0; index < items; ++index)
        {
            std::ignore = context.item(make_im_index_id(index), rect);
        }
        context.end_frame();
    }
    const Clock::time_point start = Clock::now();
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        context.begin_frame(input);
        for (uint32_t index = 0; index < items; ++index)
        {
            std::ignore = context.item(make_im_index_id(index), rect);
        }
        context.end_frame();
    }
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count() / static_cast<double>(frames);
}

} // namespace

TEST_SUITE("benchmark")
{

TEST_CASE("Benchmark: ImContext::item registration at 100 / 1k / 10k ids")
{
    MESSAGE("100 items: " << milliseconds_per_frame(100, 2000) << " ms/frame");
    MESSAGE("1k items: " << milliseconds_per_frame(1000, 200) << " ms/frame");
    MESSAGE("10k items: " << milliseconds_per_frame(10000, 20) << " ms/frame");
}

}
