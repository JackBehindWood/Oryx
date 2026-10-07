#include "doctest.h"

#include "Oryx.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;

namespace
{

const Colour RED = { 1.0f, 0.0f, 0.0f, 1.0f };
const Colour BLUE = { 0.0f, 0.0f, 1.0f, 1.0f };

} // namespace

TEST_CASE("DrawList: runs keep draw order across kinds")
{
    DrawList list;
    list.add_rect({ { 0.0f, 0.0f }, { 4.0f, 4.0f } }, RED);
    list.add_rect({ { 1.0f, 1.0f }, { 4.0f, 4.0f } }, RED);
    list.add_text({ 2.0f, 8.0f }, "AB", 16.0f, TextAlign::Left, BLUE);
    list.add_rect({ { 2.0f, 2.0f }, { 4.0f, 4.0f } }, BLUE);
    const DrawChannel& channel = list.channel(0);
    REQUIRE(channel.runs.size() == 3);
    CHECK(channel.runs[0].kind == DrawKind::Rect);
    CHECK(channel.runs[0].count == 2);
    CHECK(channel.runs[1].kind == DrawKind::Text);
    CHECK(channel.runs[2].kind == DrawKind::Rect);
    CHECK(channel.runs[2].first == 2);
    CHECK(list.text(channel.texts[0]) == "AB");
    CHECK(list.command_count() == 4);
}

TEST_CASE("DrawList: dump is deterministic text")
{
    DrawList list;
    list.set_surface(3);
    list.add_rect({ { 1.0f, 2.0f }, { 3.0f, 4.0f } }, RED);
    list.push_clip({ { 0.0f, 0.0f }, { 2.0f, 2.0f } });
    list.add_text({ 1.0f, 1.5f }, "Hi", 12.0f, TextAlign::Centre, BLUE);
    list.pop_clip();
    const std::string expected =
        "surface 3\n"
        "channel 0\n"
        "  rect [1.00 2.00 3.00 4.00] rgba(1.00 0.00 0.00 1.00)\n"
        "  text (1.00 1.50) h=12.00 align=1 \"Hi\" rgba(0.00 0.00 1.00 1.00) clip=[0.00 0.00 2.00 2.00]\n";
    CHECK(dump(list) == expected);
}

TEST_CASE("DrawList: zero-area and fully clipped commands are not recorded")
{
    DrawList list;
    list.add_rect({ { 0.0f, 0.0f }, { 0.0f, 5.0f } }, RED);
    list.add_rounded_rect({ { 0.0f, 0.0f }, { 5.0f, 0.0f } }, uniform_radius(2.0f), RED);
    list.add_border({ { 0.0f, 0.0f }, { 5.0f, 5.0f } }, {}, 0.0f, RED);
    list.add_line({ 1.0f, 1.0f }, { 2.0f, 2.0f }, 0.0f, RED);
    list.add_text({ 0.0f, 0.0f }, "", 16.0f, TextAlign::Left, RED);
    CHECK(list.command_count() == 0);
    list.push_clip({ { 0.0f, 0.0f }, { 10.0f, 10.0f } });
    list.add_rect({ { 20.0f, 20.0f }, { 5.0f, 5.0f } }, RED);
    list.add_rect({ { 5.0f, 5.0f }, { 20.0f, 20.0f } }, RED);
    list.pop_clip();
    CHECK(list.command_count() == 1);
}

TEST_CASE("DrawList: clips nest by intersection and equal clips share an index")
{
    DrawList list;
    list.push_clip({ { 0.0f, 0.0f }, { 10.0f, 10.0f } });
    list.push_clip({ { 5.0f, 5.0f }, { 10.0f, 10.0f } });
    CHECK(list.current_clip() == Rect{ { 5.0f, 5.0f }, { 5.0f, 5.0f } });
    CHECK(list.clip_depth() == 2);
    list.add_rect({ { 6.0f, 6.0f }, { 1.0f, 1.0f } }, RED);
    list.pop_clip();
    list.add_rect({ { 1.0f, 1.0f }, { 1.0f, 1.0f } }, RED);
    list.push_clip({ { -5.0f, -5.0f }, { 20.0f, 20.0f } });
    list.add_rect({ { 1.0f, 1.0f }, { 1.0f, 1.0f } }, RED);
    list.pop_clip();
    list.pop_clip();
    const DrawChannel& channel = list.channel(0);
    REQUIRE(channel.rects.size() == 3);
    CHECK(list.clip(channel.rects[0].clip) == Rect{ { 5.0f, 5.0f }, { 5.0f, 5.0f } });
    CHECK(channel.rects[1].clip == channel.rects[2].clip);
    CHECK_FALSE(list.has_clip());
}

TEST_CASE("DrawList: clip misuse throws")
{
    DrawList list;
    CHECK_THROWS_AS(list.pop_clip(), Error);
    list.push_clip({ { 0.0f, 0.0f }, { 1.0f, 1.0f } });
    CHECK_THROWS_AS(list.clear(), Error);
    list.pop_clip();
    CHECK_NOTHROW(list.clear());
}

TEST_CASE("DrawList: merge appends later channels in order")
{
    DrawList list;
    list.split_channels(3);
    list.set_channel(2);
    list.add_rect({ { 0.0f, 0.0f }, { 1.0f, 1.0f } }, BLUE);
    list.set_channel(0);
    list.add_rect({ { 0.0f, 0.0f }, { 2.0f, 2.0f } }, RED);
    list.add_text({ 0.0f, 0.0f }, "A", 16.0f, TextAlign::Left, RED);
    list.set_channel(1);
    list.add_text({ 0.0f, 0.0f }, "B", 16.0f, TextAlign::Left, BLUE);
    CHECK(list.channel_count() == 3);
    list.merge();
    CHECK(list.channel_count() == 1);
    const DrawChannel& channel = list.channel(0);
    REQUIRE(channel.runs.size() == 4);
    CHECK(channel.rects[0].colour == RED);
    CHECK(channel.rects[1].colour == BLUE);
    CHECK(list.text(channel.texts[0]) == "A");
    CHECK(list.text(channel.texts[1]) == "B");
    CHECK(channel.runs[3].kind == DrawKind::Rect);
    CHECK(channel.runs[3].first == 1);
}

TEST_CASE("DrawList: channel misuse throws")
{
    DrawList list;
    CHECK_THROWS_AS(list.split_channels(0), Error);
    CHECK_THROWS_AS(list.set_channel(1), Error);
    CHECK_THROWS_AS(list.channel(1), Error);
}

TEST_CASE("DrawList: a warm list records without allocating")
{
    DrawList list;
    auto frame = [&list]()
    {
        list.clear();
        list.split_channels(2);
        list.push_clip({ { 0.0f, 0.0f }, { 50.0f, 50.0f } });
        for (uint32_t i = 0; i < 20; ++i)
        {
            list.add_rect({ { static_cast<float>(i), 0.0f }, { 4.0f, 4.0f } }, RED);
            list.add_text({ 1.0f, 1.0f }, "label", 16.0f, TextAlign::Left, BLUE);
        }
        list.pop_clip();
        list.set_channel(1);
        list.add_border({ { 0.0f, 0.0f }, { 5.0f, 5.0f } }, uniform_radius(1.0f), 1.0f, RED);
        list.merge();
    };
    frame();
    frame();
    MemoryStats before = test::all_allocations();
    frame();
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
