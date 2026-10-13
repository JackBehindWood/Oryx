#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"
#include "unit/Scripting/ScriptingTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct SettingsRig
{
    GuiFixture f;
    Workbenches workbenches;
    SettingsWorkbench* settings = nullptr;

    SettingsRig()
    {
        reset_settings();
        f.driver.input().surface_size = { 600.0f, 500.0f };
        settings = &workbenches.add<SettingsWorkbench>();
    }

    ~SettingsRig() { reset_settings(); }

    template<typename Action>
    void once(Action&& action)
    {
        auto content = [&] { action(); };
        f.driver.run_frames(1, f.column_of(content));
    }

    void frames(uint32_t count = 3)
    {
        auto content = [this]
        {
            workbenches.update(WorkbenchFrame{ 1.0f / 60.0f, 0 });
            settings->draw_settings();
        };
        f.driver.run_frames(count, f.column_of(content));
    }
};

} // namespace

TEST_CASE("settings workbench: a valid edit applies live and an invalid one keeps its message and the old value")
{
    SettingsRig rig;
    rig.frames();
    CHECK(rig.settings->text_of("dashboard.history") == "256");

    CHECK(rig.settings->edit("dashboard.history", "32"));
    CHECK(settings_of<DashboardSettings>().history == 32);
    CHECK(rig.settings->error_of("dashboard.history").empty());

    CHECK_FALSE(rig.settings->edit("dashboard.history", "0"));
    CHECK(settings_of<DashboardSettings>().history == 32);
    CHECK_FALSE(rig.settings->error_of("dashboard.history").empty());
    CHECK(rig.settings->invalid_count() == 1);
    rig.frames();
    CHECK(rig.settings->invalid_count() == 1);

    CHECK(rig.settings->edit("dashboard.history", "48"));
    CHECK(rig.settings->invalid_count() == 0);
    CHECK_THROWS_AS(rig.settings->edit("dashboard.nothing", "1"), SettingsError);
}

TEST_CASE("settings workbench: edits count as unsaved until Save, and Revert returns to the files")
{
    SettingsRig rig;
    TempDir dir;
    const std::filesystem::path base = dir.write("oryx.yaml", "dashboard:\n  history: 100\n");
    std::string argument = "--settings=" + base.string();
    std::string program = "app";
    char* pointers[] = { program.data(), argument.data() };
    load_settings(ApplicationCommandLineArgs{ 2, pointers });

    rig.frames();
    CHECK(rig.settings->modified_count() == 0);
    CHECK(rig.settings->edit("dashboard.history", "7"));
    CHECK(rig.settings->modified_count() == 1);

    rig.once([&] { rig.settings->save(); });
    CHECK(rig.settings->modified_count() == 0);
    CHECK(std::filesystem::exists(dir.path() / "oryx.local.yaml"));

    CHECK(rig.settings->edit("dashboard.history", "9"));
    rig.once([&] { rig.settings->revert(); });
    CHECK(settings_of<DashboardSettings>().history == 7);
    CHECK(rig.settings->modified_count() == 0);
}

TEST_CASE("settings workbench: a reload from elsewhere refreshes the rows")
{
    SettingsRig rig;
    rig.frames();
    set_setting("dashboard.history", "11");
    rig.frames();
    CHECK(rig.settings->text_of("dashboard.history") == "11");
    reset_settings();
    rig.frames();
    CHECK(rig.settings->text_of("dashboard.history") == "256");
}

TEST_CASE("settings workbench: the search box narrows the rows without losing state")
{
    SettingsRig rig;
    rig.settings->set_search("history");
    rig.frames();
    CHECK(rig.settings->text_of("dashboard.history") == "256");
    rig.settings->set_search("");
    rig.frames();
}

TEST_CASE("settings workbench: a warm frame allocates nothing")
{
    SettingsRig rig;
    rig.frames(8);
    const MemoryStats before = all_allocations();
    rig.frames(3);
    CHECK(memory_delta(before, all_allocations()).allocation_count == 0);
}

TEST_CASE("settings workbench: creating and dropping the workbench leaves no allocation behind")
{
    {
        SettingsRig warm;
        warm.frames(2);
    }
    const MemoryStats before = all_allocations();
    {
        SettingsRig rig;
        rig.frames(2);
        rig.settings->edit("dashboard.history", "5");
    }
    const MemoryStats delta = memory_delta(before, all_allocations());
    CHECK(delta.allocation_count == delta.deallocation_count);
}
