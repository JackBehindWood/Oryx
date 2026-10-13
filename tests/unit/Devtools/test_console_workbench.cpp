#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_LOG_CAPTURE

namespace
{

struct ConsoleRig
{
    GuiFixture f;
    Workbenches workbenches;
    bool drawer = false;
    ConsoleWorkbench* console = nullptr;

    ConsoleRig()
    {
        log_capture::clear();
        f.driver.input().surface_size = { 600.0f, 400.0f };
        console = &workbenches.add<ConsoleWorkbench>(drawer);
    }

    ~ConsoleRig() { log_capture::clear(); }

    // column_of keeps a reference to what it wraps, so the content must outlive the call that uses it.
    template<typename Action>
    void with_body(Action&& action)
    {
        auto content = [this]
        {
            workbenches.update(WorkbenchFrame{ 1.0f / 60.0f, 0 });
            console->draw_console();
        };
        action(f.column_of(content));
    }

    void frames(uint32_t count = 4)
    {
        with_body([&](auto&& build) { f.driver.run_frames(count, build); });
    }

    void click(const Vec2f& at)
    {
        with_body([&](auto&& build) { f.driver.click(at, build); });
    }
};

} // namespace

TEST_CASE("console: the captured lines show as rows with their level and logger")
{
    ConsoleRig rig;
    log_capture::record(2, LogSource::App, "hello");
    log_capture::record(4, LogSource::Core, "broken");
    rig.frames();
    CHECK(rig.console->visible_count() == 2);
    CHECK(rig.f.find_text("I APP   hello") != nullptr);
    CHECK(rig.f.find_text("E ORYX  broken") != nullptr);
    CHECK(rig.console->errors() == 1);
    CHECK(rig.console->warnings() == 0);
}

TEST_CASE("console: the level and the search filter the rows")
{
    ConsoleRig rig;
    log_capture::record(0, LogSource::App, "trace line");
    log_capture::record(2, LogSource::App, "info line");
    log_capture::record(3, LogSource::App, "warn line");
    log_capture::record(4, LogSource::App, "error line");
    rig.frames();
    CHECK(rig.console->visible_count() == 4);
    CHECK(rig.console->warnings() == 1);

    rig.console->set_level(3);
    rig.frames(2);
    CHECK(rig.console->visible_count() == 2);
    CHECK(rig.f.find_text("I APP   info line") == nullptr);

    rig.console->set_level(0);
    rig.console->set_search("LINE");
    CHECK(rig.console->visible_count() == 4);
    rig.console->set_search("warn");
    CHECK(rig.console->visible_count() == 1);
    rig.console->set_search("nothing like it");
    CHECK(rig.console->visible_count() == 0);
}

TEST_CASE("console: a new line shows without any other change and Clear empties the list")
{
    ConsoleRig rig;
    rig.frames();
    CHECK(rig.console->visible_count() == 0);
    log_capture::record(2, LogSource::App, "late");
    rig.frames(2);
    CHECK(rig.console->visible_count() == 1);
    rig.console->run_line("clear");
    rig.frames(2);
    CHECK(rig.console->visible_count() == 0);
}

#ifdef OX_ENABLE_CONSOLE_COMMANDS
TEST_CASE("console: a command line echoes, runs and prints its output as rows")
{
    ConsoleRig rig;
    rig.console->run_line("help clear");
    rig.frames(2);
    CHECK(rig.f.find_text("I APP   > help clear") != nullptr);
    CHECK(rig.console->visible_count() >= 2);
    rig.console->run_line("no.such.thing");
    rig.frames(2);
    CHECK(rig.console->errors() >= 1);
}

TEST_CASE("console: workbench commands run by name, and the console's own are listed")
{
    ConsoleRig rig;
    rig.console->run_line("console.toggle");
    CHECK(rig.drawer);
    rig.console->run_line("console.toggle");
    CHECK_FALSE(rig.drawer);
    rig.console->run_line("console.clear");
    rig.frames(2);
    CHECK(rig.console->visible_count() == 0);
}
#endif

TEST_CASE("console: the status item shows the counts and flips the drawer")
{
    ConsoleRig rig;
    log_capture::record(3, LogSource::App, "w");
    log_capture::record(4, LogSource::App, "e");
    log_capture::record(5, LogSource::App, "c");
    rig.frames();
    auto status_body = [&] { rig.workbenches.status(); };
    auto status = rig.f.column_of(status_body);
    rig.f.driver.run_frames(3, status);
    CHECK(rig.f.find_text("2 errors") != nullptr);
    CHECK(rig.f.find_text("1 warnings") != nullptr);
    const LayoutNode* toggle = rig.f.find_text("Console");
    REQUIRE(toggle != nullptr);
    rig.f.driver.click(rect_centre(toggle->rect), status);
    CHECK(rig.drawer);
}

TEST_CASE("console: clicking a row selects it and Cmd+C copies the selected lines")
{
    ConsoleRig rig;
    log_capture::record(2, LogSource::App, "first");
    log_capture::record(2, LogSource::App, "second");
    log_capture::record(2, LogSource::App, "third");
    rig.frames(5);
    const LayoutNode* first = rig.f.find_text("I APP   first");
    const LayoutNode* second = rig.f.find_text("I APP   second");
    REQUIRE((first != nullptr && second != nullptr));
    rig.click(rect_centre(first->rect));
    rig.f.driver.chord(ImKey::C);
    rig.frames(1);
    CHECK(rig.f.context.output().copy_text == "first\n");

    rig.f.driver.move_to(rect_centre(second->rect));
    rig.f.driver.input().keys.shift = true;
    rig.click(rect_centre(second->rect));
    rig.f.driver.input().keys.shift = false;
    rig.f.driver.chord(ImKey::C);
    rig.frames(1);
    CHECK(rig.f.context.output().copy_text == "first\nsecond\n");
}

TEST_CASE("console: following the tail pauses when the wheel moves over the rows")
{
    ConsoleRig rig;
    for (uint32_t i = 0; i < 40; ++i)
    {
        log_capture::record(2, LogSource::App, "line " + std::to_string(i));
    }
    rig.frames(5);
    CHECK(rig.console->following());
    const LayoutNode* row = rig.f.find_text("I APP   line 39");
    REQUIRE(row != nullptr);
    rig.f.driver.move_to(rect_centre(row->rect));
    rig.f.driver.wheel({ 0.0f, 1.0f });
    rig.frames(1);
    CHECK_FALSE(rig.console->following());
}

TEST_CASE("console: a warm frame allocates nothing")
{
    ConsoleRig rig;
    for (uint32_t i = 0; i < 30; ++i)
    {
        log_capture::record(2, LogSource::App, "line " + std::to_string(i));
    }
    rig.frames(8);
    const MemoryStats before = all_allocations();
    rig.frames(3);
    CHECK(memory_delta(before, all_allocations()).allocation_count == 0);
}

#endif
