#include "doctest.h"

#include "Oryx.h"
#include "unit/Game/DummyGame.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct DashboardRig
{
    GuiFixture f;
    Workbenches workbenches;
    Dashboard* dashboard = nullptr;

    explicit DashboardRig(std::initializer_list<const char*> views = { "probabilities", "values" })
    {
        f.driver.input().surface_size = { 500.0f, 500.0f };
        dashboard = &workbenches.add<Dashboard>();
        for (const char* view : views)
        {
            dashboard->add_view(view);
        }
    }

    void frames(uint32_t count = 4)
    {
        f.driver.run_frames(count, f.frame_of(
            [this]
            {
                workbenches.update(WorkbenchFrame{ 1.0f / 60.0f, 0 });
                gui::PanelHostScope host;
                workbenches.draw();
            }));
    }

    void add_decision()
    {
        UniquePtr<IGame> game = GameRegistry::create("tictactoe");
        REQUIRE(game != nullptr);
        UniquePtr<IState> position = game->new_initial_state();
        ActionList actions = position->legal_actions();
        Decision decision;
        decision.player = position->current_player();
        decision.chosen = actions[0];
        for (ActionId action : actions)
        {
            set_probability(decision, action, 1.0 / static_cast<double>(actions.size()));
            set_value(decision, action, action == decision.chosen ? 0.5 : -0.25);
        }
        dashboard->feed().on_decision(*position, decision);
    }
};

const gui::PanelDesc* panel_named(std::string_view name)
{
    return gui::find_panel(gui::panels(), gui::make_panel_id(name));
}

} // namespace

TEST_CASE("dashboard workbench: each view is a panel of the dashboard group, an unknown id is skipped")
{
    DashboardRig rig({ "values", "no/such/view", "probabilities" });
    CHECK(rig.dashboard->view_count() == 2);
    CHECK(panel_named("viewport/0") != nullptr);
    UniquePtr<IDashboardView> values = DashboardViewRegistry::create("values");
    REQUIRE(values != nullptr);
    CHECK(panel_named("dashboard/" + std::string(values->title())) == nullptr);
    bool found_group = false;
    for (uint32_t i = 0; i < gui::panels().count; ++i)
    {
        found_group = found_group || gui::panels().descs[i].group == gui::make_panel_id("dashboard");
    }
    CHECK(found_group);
}

TEST_CASE("dashboard workbench: views on one side are tabbed together and a bottom view docks apart")
{
    DashboardRig rig({ "trace", "decision", "stats" });
    const gui::PanelDesc* trace = panel_named("dashboard/trace");
    const gui::PanelDesc* decision = panel_named("dashboard/decision");
    const gui::PanelDesc* stats = panel_named("dashboard/stats");
    REQUIRE((trace != nullptr && decision != nullptr && stats != nullptr));
    CHECK(trace->dock_side == gui::DropZone::Bottom);
    CHECK_FALSE(gui::is_valid(trace->dock_near));
    CHECK_FALSE(gui::is_valid(trace->dock_tabbed_with));
    CHECK(decision->dock_side == gui::DropZone::Right);
    CHECK(decision->dock_size == doctest::Approx(360.0f));
    CHECK(stats->dock_tabbed_with == decision->id);
}

TEST_CASE("dashboard workbench: a custom view is added in place and drawn when its panel is selected")
{
    struct CountingView final : IDashboardView
    {
        explicit CountingView(uint32_t& draws)
            : count(draws)
        {
        }
        std::string_view title() const override { return "Counting"; }
        void draw(DashboardModel&) override { ++count; }
        uint32_t& count;
    };
    uint32_t draws = 0;
    DashboardRig rig({});
    rig.dashboard->add_view<CountingView>(draws);
    rig.frames(6);
    CHECK(draws > 0);
}

TEST_CASE("dashboard workbench: the dashboard opens by default, closes with its group and the board panel stays")
{
    DashboardRig rig;
    rig.frames();
    CHECK(rig.dashboard->open());
    rig.dashboard->set_open(false);
    rig.frames();
    CHECK_FALSE(rig.dashboard->open());
    CHECK(gui::is_panel_open("viewport/0"));
}

TEST_CASE("dashboard workbench: the status shows the match and decision counts")
{
    DashboardRig rig;
    rig.add_decision();
    rig.f.driver.frame(rig.f.frame_of([&] { rig.workbenches.status(); }));
    CHECK(rig.f.find_text("Match 0 \xC2\xB7 1 decisions") != nullptr);
}

TEST_CASE("dashboard workbench: the annotation flags start from the setting")
{
    DashboardRig rig;
    CHECK(rig.dashboard->annotation_flags().heat);
    CHECK(rig.dashboard->annotation_flags().chosen);
    CHECK_FALSE(rig.dashboard->annotation_flags().value);
}

TEST_CASE("dashboard workbench: a warm frame with a populated feed allocates nothing")
{
    DashboardRig rig;
    rig.add_decision();
    rig.add_decision();
    rig.frames(8);
    const MemoryStats before = test::all_allocations();
    rig.frames(3);
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

#ifdef OX_ENABLE_LOG_CAPTURE
TEST_CASE("dashboard workbench: the trace panel and the console drawn in one frame stay stable and warm")
{
    log_capture::clear();
    bool drawer = true;
    DashboardRig rig({ "trace", "decision" });
    ConsoleWorkbench& console = rig.workbenches.add<ConsoleWorkbench>(drawer);
    for (uint32_t i = 0; i < 40; ++i)
    {
        rig.add_decision();
        log_capture::record(2, LogSource::App, "line " + std::to_string(i));
    }
    rig.frames(8);
    for (uint32_t i = 0; i < 20; ++i)
    {
        rig.f.driver.move_to({ 100.0f + static_cast<float>(i) * 10.0f, 120.0f + static_cast<float>(i) * 7.0f });
        rig.f.driver.wheel({ 0.0f, i % 2 == 0 ? -1.0f : 1.0f });
        rig.frames(1);
    }
    CHECK(console.visible_count() > 0);
    rig.frames(8);
    const MemoryStats before = test::all_allocations();
    rig.frames(3);
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
    log_capture::clear();
}
#endif

#ifdef OX_ENABLE_LOG_CAPTURE
namespace
{

float drag_trace_splitter(bool console_open)
{
    log_capture::clear();
    bool drawer = console_open;
    DashboardRig rig({ "trace", "decision" });
    ConsoleWorkbench& console = rig.workbenches.add<ConsoleWorkbench>(drawer);
    rig.f.driver.input().surface_size = { 800.0f, 600.0f };
    const auto build = rig.f.frame_of(
        [&]
        {
            rig.workbenches.update(WorkbenchFrame{ 1.0f / 60.0f, 0 });
            if (gui::AreaScope bar("status", { .anchor = gui::AreaAnchor::Bottom }); bar)
                gui::label("status");
            gui::AreaOptions area;
            area.anchor = gui::AreaAnchor::Bottom;
            area.size = fixed(200.0f);
            area.resizable = true;
            area.open = &drawer;
            if (gui::AreaScope drawer_area("console_drawer", area); drawer_area)
                console.draw_console();
            if (gui::AreaScope body("body"); body)
            {
                gui::PanelHostScope host;
                rig.workbenches.draw();
            }
        });
    rig.f.driver.run_frames(6, build);
    const gui::SolvedLayout& solved = rig.f.context.dock_view().solved;
    float moved = 0.0f;
    for (uint32_t i = 0; i < solved.node_count; ++i)
    {
        const Rect gap = solved.nodes[i].splitter;
        if (is_empty(gap) || gap.size[0] < gap.size[1])
            continue;
        const Vec2f at = rect_centre(gap);
        rig.f.driver.drag(at, at + Vec2f(0.0f, -40.0f), 6, build);
        const Rect after = rig.f.context.dock_view().solved.nodes[i].splitter;
        moved = gap.min[1] - after.min[1];
        break;
    }
    log_capture::clear();
    return moved;
}

} // namespace

TEST_CASE("dashboard workbench: the trace splitter still grows the panel with the console area open")
{
    CHECK(drag_trace_splitter(false) > 20.0f);
    CHECK(drag_trace_splitter(true) > 0.0f);
}
#endif
