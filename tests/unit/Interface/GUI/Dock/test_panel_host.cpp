#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

// Nodes of make_sample: 0 split (ratio 0.7), 1 tabs [a b*], 2 tabs [vp*].
struct HostScene
{
    GuiFixture f;
    DockLayout layout;
    PanelHostOptions options;
    bool reversed = false;
    bool a_visible = false;
    bool b_visible = false;
    uint32_t rows = 0;

    explicit HostScene(float width = 800.0f, float height = 600.0f)
    {
        f.driver.input().surface_size = { width, height };
        panel_host().panels = make_panels();
        layout = make_sample(panel_host().panels);
    }

    PanelHostState& panel_host() { return f.context.panel_host(); }
    const SolvedLayout& solved() { return panel_host().solved; }

    void draw()
    {
        PanelHostScope host(layout, options);
        if (reversed)
            draw_b();
        {
            PanelScope a("a");
            a_visible = a.visible();
            if (a.visible())
                gui::label("A");
        }
        if (!reversed)
            draw_b();
    }

    void draw_b()
    {
        PanelScope b("b");
        b_visible = b.visible();
        if (b.visible())
        {
            for (uint32_t i = 0; i < rows; ++i)
                gui::label("row");
        }
    }

    void frames(uint32_t count = 3)
    {
        auto build = f.frame_of([this] { draw(); });
        f.driver.run_frames(count, build);
    }

    template<typename Action>
    void act(Action&& action)
    {
        auto build = f.frame_of([this] { draw(); });
        action(build);
    }

    void click(const Vec2f& at)
    {
        act([&](auto& build) { f.driver.click(at, build); });
    }

    void drag(const Vec2f& from, const Vec2f& to)
    {
        act([&](auto& build) { f.driver.drag(from, to, 4, build); });
    }
};

Vec2f centre(const Rect& rect) { return Vec2f(rect.min[0] + rect.size[0] * 0.5f, rect.min[1] + rect.size[1] * 0.5f); }

void check_near(const Rect& a, const Rect& b)
{
    CHECK(a.min[0] == doctest::Approx(b.min[0]).epsilon(0.001));
    CHECK(a.min[1] == doctest::Approx(b.min[1]).epsilon(0.001));
    CHECK(a.size[0] == doctest::Approx(b.size[0]).epsilon(0.001));
    CHECK(a.size[1] == doctest::Approx(b.size[1]).epsilon(0.001));
}

Rect close_rect_of(HostScene& scene, const Rect& tab)
{
    const float pad = scene.f.context.gui_theme().tab.padding.right * 0.5f;
    return Rect{ Vec2f(tab.min[0] + tab.size[0] - pad - 14.0f, tab.min[1] + (tab.size[1] - 14.0f) * 0.5f), Vec2f(14.0f, 14.0f) };
}

} // namespace

TEST_CASE("panel host: the first frame draws nothing and later frames place bodies at the solved rects")
{
    HostScene scene;
    scene.frames(1);
    CHECK_FALSE(scene.b_visible);
    CHECK(is_empty(viewport_rect("vp")));

    scene.frames(2);
    REQUIRE(scene.b_visible);
    CHECK_FALSE(scene.a_visible);
    const Rect body = scene.solved().nodes[1].body;
    Rect placed;
    REQUIRE(scene.f.context.layout_rect(scene.f.context.id("b"), placed));
    check_near(placed, body);
    check_near(viewport_rect("vp"), scene.solved().nodes[2].body);
}

TEST_CASE("panel host: a resize converges in one more frame and begin order does not matter")
{
    HostScene scene;
    scene.frames();
    scene.f.driver.input().surface_size = { 500.0f, 400.0f };
    scene.frames(3);
    Rect forward;
    REQUIRE(scene.f.context.layout_rect(scene.f.context.id("b"), forward));
    check_near(forward, scene.solved().nodes[1].body);
    CHECK(scene.solved().surface_rect.size[0] == doctest::Approx(500.0f));

    scene.reversed = true;
    scene.frames(3);
    Rect reversed;
    REQUIRE(scene.f.context.layout_rect(scene.f.context.id("b"), reversed));
    check_near(reversed, forward);
}

TEST_CASE("panel host: a body clips and scrolls")
{
    HostScene scene;
    scene.rows = 60;
    scene.frames();
    REQUIRE(scene.b_visible);
    float top = 0.0f;
    for (uint32_t i = 0; i < scene.f.context.layout().node_count(); ++i)
        if (scene.f.context.layout().node(i).paint.text == "row")
        {
            top = scene.f.context.layout().node(i).rect.min[1];
            break;
        }
    scene.f.driver.move_to(centre(scene.solved().nodes[1].body));
    scene.f.driver.wheel({ 0.0f, -3.0f });
    scene.frames(2);
    float scrolled = top;
    for (uint32_t i = 0; i < scene.f.context.layout().node_count(); ++i)
        if (scene.f.context.layout().node(i).paint.text == "row")
        {
            scrolled = scene.f.context.layout().node(i).rect.min[1];
            break;
        }
    CHECK(scrolled < top);
}

TEST_CASE("panel host: clicking a tab selects it and reports the change")
{
    HostScene scene;
    scene.frames();
    CHECK(scene.layout.nodes[1].selected == 1);
    scene.click(centre(scene.solved().nodes[1].tab_rects[0]));
    CHECK(scene.layout.nodes[1].selected == 0);
    CHECK(focused_panel() == pid("a"));
    scene.frames();
    CHECK(scene.a_visible);
    CHECK_FALSE(scene.b_visible);
}

TEST_CASE("panel host: the close button closes the panel and records its home")
{
    HostScene scene;
    scene.frames();
    const Rect tab = scene.solved().nodes[1].tab_rects[0];
    scene.click(centre(close_rect_of(scene, tab)));
    CHECK_FALSE(is_open(scene.layout, pid("a")));
    CHECK(scene.layout.home_count == 1);
    CHECK(scene.layout.homes[0].panel == pid("a"));
    CHECK_NOTHROW(validate(scene.layout));
}

TEST_CASE("panel host: a panel without the close flag has no close button")
{
    HostScene scene;
    set_flags(scene.panel_host().panels, "a", panel_flag::all & ~panel_flag::close);
    scene.frames();
    const Rect tab = scene.solved().nodes[1].tab_rects[0];
    scene.click(centre(close_rect_of(scene, tab)));
    CHECK(is_open(scene.layout, pid("a")));
    CHECK(scene.layout.nodes[1].selected == 0);
}

TEST_CASE("panel host: the chevron collapses a node and hides its body")
{
    HostScene scene;
    scene.frames();
    const Rect chevron = scene.solved().nodes[1].collapse_button;
    REQUIRE_FALSE(is_empty(chevron));
    scene.click(centre(chevron));
    CHECK(scene.layout.nodes[1].collapsed == 1);
    scene.frames();
    CHECK_FALSE(scene.b_visible);
    CHECK(is_empty(scene.solved().nodes[1].body));
}

TEST_CASE("panel host: a panel without the collapse flag has no chevron")
{
    HostScene scene;
    set_flags(scene.panel_host().panels, "b", panel_flag::all & ~panel_flag::collapse);
    scene.frames();
    CHECK(is_empty(scene.solved().nodes[1].collapse_button));
}

TEST_CASE("panel host: dragging a splitter edits the ratio, points or both modes within the clamps")
{
    HostScene scene;
    scene.frames();
    const float before = scene.layout.nodes[0].ratio;
    Rect gap = scene.solved().nodes[0].splitter;
    scene.drag(centre(gap), centre(gap) + Vec2f(-100.0f, 0.0f));
    CHECK(scene.layout.nodes[0].ratio < before);

    scene.layout.nodes[0].mode = DockSizeMode::FixedSecond;
    scene.layout.nodes[0].points = 200.0f;
    scene.frames();
    gap = scene.solved().nodes[0].splitter;
    scene.drag(centre(gap), centre(gap) + Vec2f(-50.0f, 0.0f));
    CHECK(scene.layout.nodes[0].points > 200.0f);
    CHECK_NOTHROW(validate(scene.layout));

    scene.layout.nodes[0].mode = DockSizeMode::FixedFirst;
    scene.layout.nodes[0].points = 300.0f;
    scene.frames();
    gap = scene.solved().nodes[0].splitter;
    scene.drag(centre(gap), centre(gap) + Vec2f(40.0f, 0.0f));
    CHECK(scene.layout.nodes[0].points > 300.0f);

    scene.drag(centre(gap), Vec2f(-5000.0f, 0.0f));
    CHECK(scene.layout.nodes[0].points >= 0.0f);
    CHECK_NOTHROW(validate(scene.layout));
}

TEST_CASE("panel host: a splitter next to a non-resizable side does not move")
{
    HostScene scene;
    set_flags(scene.panel_host().panels, "vp", panel_flag::reorder_in_host | panel_flag::dock_elsewhere);
    scene.frames();
    const DockLayout before = scene.layout;
    const Rect gap = scene.solved().nodes[0].splitter;
    scene.drag(centre(gap), centre(gap) + Vec2f(-100.0f, 0.0f));
    CHECK(equal(scene.layout, before));
}

TEST_CASE("panel host: the wheel over an overflowing strip steps the selected tab")
{
    HostScene scene(300.0f, 400.0f);
    for (const char* name : { "c", "d", "e", "f", "g", "h" })
        require_applied(dock_panel(scene.layout, scene.panel_host().panels, pid(name), 1, DropZone::Centre));
    scene.frames();
    const SolvedNode& node = scene.solved().nodes[1];
    REQUIRE(node.visible_count < scene.layout.nodes[1].count);
    const uint8_t selected = scene.layout.nodes[1].selected;
    scene.f.driver.move_to(centre(node.strip));
    scene.f.driver.wheel({ 0.0f, 1.0f });
    scene.frames(1);
    CHECK(scene.layout.nodes[1].selected == selected - 1);
}

TEST_CASE("panel host: a press in a body or tab moves the focus seam")
{
    HostScene scene;
    scene.frames();
    scene.click(centre(scene.solved().nodes[1].body));
    CHECK(focused_panel() == pid("b"));
    scene.click(centre(scene.solved().nodes[1].tab_rects[0]));
    CHECK(focused_panel() == pid("a"));
    set_focused_panel(pid("vp"));
    CHECK(focused_panel() == pid("vp"));
}

TEST_CASE("panel host: strips, splitters and bodies claim the pointer but a viewport rect does not")
{
    HostScene scene;
    scene.frames();
    const SolvedLayout& solved = scene.solved();
    const Vec2f spots[] = { centre(solved.nodes[1].strip), centre(solved.nodes[1].body), centre(solved.nodes[0].splitter) };
    for (const Vec2f& spot : spots)
    {
        scene.f.driver.move_to(spot);
        scene.frames(1);
        CHECK(scene.f.context.wants_mouse());
    }
    scene.f.driver.move_to(centre(viewport_rect("vp")));
    scene.frames(1);
    CHECK_FALSE(scene.f.context.wants_mouse());
}

TEST_CASE("panel host: an unregistered id in the layout is skipped and reported once")
{
    HostScene scene;
    DockNode& tabs = scene.layout.nodes[1];
    tabs.tabs[tabs.count++] = PanelId{ 0xBEEF };
    scene.frames(6);
    CHECK(scene.panel_host().reported_count == 1);
    CHECK(scene.b_visible);
    CHECK(scene.layout.nodes[1].count == 3);
}

TEST_CASE("panel host: an invalid layout is replaced by the last good one without throwing")
{
    HostScene scene;
    scene.frames();
    const DockLayout good = scene.layout;
    scene.layout.nodes[0].first = 99;
    CHECK_NOTHROW(scene.frames(2));
    CHECK(equal(scene.layout, good));
}

TEST_CASE("panel host: a layout without the required viewport is replaced by the last good one")
{
    HostScene scene;
    scene.options.require_viewport = true;
    scene.frames();
    const DockLayout good = scene.layout;
    DockLayout without;
    require_applied(dock_panel(without, scene.panel_host().panels, pid("a"), k_dock_root, DropZone::Centre));
    scene.layout = without;
    scene.frames();
    CHECK(equal(scene.layout, good));
}

TEST_CASE("panel host: an empty surface offers a reset to the default layout")
{
    HostScene scene;
    const DockLayout defaults = scene.layout;
    scene.layout = DockLayout{};
    scene.options.default_layout = &defaults;
    scene.frames();
    const LayoutNode* hint = scene.f.find_text("No panels open");
    REQUIRE(hint != nullptr);
    const LayoutNode* reset = scene.f.find_text("Reset layout");
    REQUIRE(reset != nullptr);
    scene.click(centre(reset->rect));
    CHECK(equal(scene.layout, defaults));
}

TEST_CASE("panel options map onto the six panel flags")
{
    HostScene scene;
    PanelOptions options;
    options.no_dock = true;
    options.no_close = true;
    CHECK(register_panel("x", options));
    CHECK_FALSE(register_panel("x", options));
    CHECK(find_panel(panels(), pid("x"))->flags.bits == (panel_flag::all & ~(panel_flag::dock_elsewhere | panel_flag::close)));

    PanelOptions viewport;
    viewport.kind = PanelKind::Viewport;
    viewport.no_resize = true;
    CHECK(register_panel("y", viewport));
    CHECK(find_panel(panels(), pid("y"))->flags.bits == (panel_flag::reorder_in_host | panel_flag::dock_elsewhere));
}

TEST_CASE("begin_panel: outside a host it is the plain container, and misuse throws")
{
    GuiFixture f;
    f.driver.frame(
        [&]
        {
            CHECK(gui::begin_panel("plain"));
            gui::label("inside");
            gui::end_panel();
            CHECK_THROWS_AS(gui::end_panel(), Error);
            CHECK_THROWS_AS(gui::end_panel_host(), Error);
        });
}

TEST_CASE("begin_panel: a plain container nested in a dock body still pairs")
{
    HostScene scene;
    scene.frames();
    bool nested = false;
    auto build = scene.f.frame_of(
        [&]
        {
            PanelHostScope host(scene.layout);
            PanelScope b("b");
            if (b.visible())
            {
                nested = gui::begin_panel("inner");
                gui::end_panel();
            }
        });
    scene.f.driver.run_frames(2, build);
    CHECK(nested);
}

TEST_CASE("panel host: warm frames allocate nothing")
{
    HostScene scene;
    scene.rows = 10;
    scene.frames(6);
    MemoryStats before = test::all_allocations();
    scene.frames(1);
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
