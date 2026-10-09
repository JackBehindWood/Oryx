#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

// Nodes of make_sample: 0 split h (0.7), 1 tabs [a b*], 2 tabs [vp*].
struct ContextRig
{
    GuiFixture f;

    ContextRig()
    {
        f.driver.input().surface_size = { 800.0f, 600.0f };
        DockModel& m = model();
        m.panels = make_panels();
        m.layout = make_sample(m.panels);
        settle_dock(m);
        frames(4);
    }

    DockModel& model() { return f.context.dock_model(); }

    void draw()
    {
        PanelHostScope host;
        for (const char* name : { "a", "b", "vp" })
        {
            PanelScope panel(name);
            if (panel.visible())
                gui::label("body");
        }
    }

    void frames(uint32_t count)
    {
        auto build = f.frame_of([this] { draw(); });
        f.driver.run_frames(count, build);
    }

    void click(const Vec2f& at)
    {
        auto build = f.frame_of([this] { draw(); });
        f.driver.click(at, build);
    }
};

// A bare GuiContext would load and flush the real gui-layout.yaml; this keeps it in memory.
struct MemoryOnlyLayout
{
    GuiSettings saved = settings_of<GuiSettings>();

    MemoryOnlyLayout() { update_settings<GuiSettings>([](GuiSettings& settings) { settings.layout_file.clear(); settings.docking = true; }); }
    ~MemoryOnlyLayout() { update_settings<GuiSettings>([this](GuiSettings& settings) { settings = saved; }); }
};

Vec2f centre_of(const Rect& rect) { return Vec2f(rect.min[0] + rect.size[0] * 0.5f, rect.min[1] + rect.size[1] * 0.5f); }

PanelOptions viewport_options()
{
    PanelOptions options;
    options.kind = PanelKind::Viewport;
    return options;
}

void register_hinted_panels()
{
    PanelOptions board;
    board.title = "Board";
    board.kind = PanelKind::Viewport;
    REQUIRE(register_panel("viewport/0", board));
    PanelOptions first;
    first.title = "First";
    first.group = "views";
    first.dock_near = "viewport/0";
    first.dock_side = DropZone::Right;
    first.dock_size = 360.0f;
    first.order = 1;
    REQUIRE(register_panel("view/first", first));
    PanelOptions second;
    second.title = "Second";
    second.group = "views";
    second.dock_tabbed_with = "view/first";
    second.order = 2;
    REQUIRE(register_panel("view/second", second));
}

}

TEST_CASE("dock context: a press on the viewport focuses it without the owner's help")
{
    ContextRig rig;
    focus_panel("a");
    REQUIRE(is_panel_focused("a"));
    rig.click(centre_of(panel_rect("vp")));
    CHECK(is_panel_focused("vp"));
}

TEST_CASE("dock context: gui.docking off lets the viewport fill the host and hides every other panel")
{
    ContextRig rig;
    CHECK_FALSE(is_empty(panel_rect("b")));
    update_settings<GuiSettings>([](GuiSettings& settings) { settings.docking = false; });
    rig.frames(4);
    const Rect board = viewport_rect("vp");
    REQUIRE_FALSE(is_empty(board));
    CHECK(board.size[0] > 700.0f);
    CHECK(is_empty(panel_rect("b")));
    CHECK(is_empty(panel_rect("a")));
    update_settings<GuiSettings>([](GuiSettings& settings) { settings.docking = true; });
    rig.frames(4);
    CHECK_FALSE(is_empty(panel_rect("b")));
}

TEST_CASE("dock context: turning docking off mid-drag cancels the drag")
{
    ContextRig rig;
    rig.model().drag.source = DragSource::Tab;
    rig.model().drag.panel = pid("b");
    update_settings<GuiSettings>([](GuiSettings& settings) { settings.docking = false; });
    rig.frames(2);
    CHECK(rig.model().drag.source == DragSource::None);
}

TEST_CASE("dock context: an empty layout_file keeps the layout in memory, docking off reads no file")
{
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "oryx_dock_context_off.yaml";
    std::filesystem::remove(file);
    update_settings<GuiSettings>([&file](GuiSettings& settings) { settings.layout_file = file; settings.docking = false; });
    {
        GuiContext context;
        ContextScope<GuiContext> scope(context);
        REQUIRE(register_panel("viewport/0", viewport_options()));
        CHECK(is_panel_open("viewport/0"));
        CHECK(context.dock_model().load == DockLoad::DefaultOnly);
        CHECK(context.dock_model().autosave == nullptr);
    }
    CHECK_FALSE(std::filesystem::exists(file));
    {
        update_settings<GuiSettings>([](GuiSettings& settings) { settings.layout_file.clear(); settings.docking = true; });
        GuiContext context;
        ContextScope<GuiContext> scope(context);
        REQUIRE(register_panel("viewport/0", viewport_options()));
        REQUIRE(register_panel("view/x"));
        CHECK(is_panel_open("view/x"));
        CHECK(set_panel_open("view/x", false).applied);
        CHECK(context.dock_model().autosave == nullptr);
    }
    CHECK_FALSE(std::filesystem::exists(file));
    reset_settings();
}

TEST_CASE("dock verbs: by-name edits obey the permission set, report why, and are undoable")
{
    ContextRig rig;
    CHECK(is_panel_open("a"));
    CHECK_FALSE(is_panel_open("nope"));
    CHECK(set_panel_open("nope", true).reason == DockReason::UnknownPanel);
    CHECK(select_panel("nope").reason == DockReason::UnknownPanel);

    const DockLayout initial = dock_layout();
    REQUIRE(set_panel_open("a", false).applied);
    CHECK_FALSE(is_panel_open("a"));
    CHECK(set_panel_open("a", false).reason == DockReason::AlreadyClosed);
    rig.frames(2);
    undo_layout();
    rig.frames(2);
    CHECK(is_panel_open("a"));
    CHECK(equal(dock_layout(), initial));

    REQUIRE(dock_panel("b", "vp", DropZone::Bottom).applied);
    CHECK(dock_panel("b", "gone", DropZone::Centre).reason == DockReason::TargetInvalid);
    REQUIRE(float_panel("a", Rect{ Vec2f(100.0f, 100.0f), Vec2f(200.0f, 150.0f) }).applied);
    CHECK_NOTHROW(validate(dock_layout(), rig.model().panels, ValidateFlags{ true }));

    DockResult pinned = float_panel("p", Rect{ Vec2f(10.0f, 10.0f), Vec2f(100.0f, 100.0f) });
    CHECK_FALSE(pinned.applied);
    CHECK(pinned.reason != DockReason::None);
}

TEST_CASE("dock verbs: select and collapse act on the panel's tab stack")
{
    ContextRig rig;
    REQUIRE(select_panel("a").applied);
    CHECK(dock_layout().nodes[1].selected == 0);
    CHECK(select_panel("a").reason == DockReason::NoChange);
    REQUIRE(set_panel_collapsed("a", true).applied);
    CHECK(dock_layout().nodes[1].collapsed == 1);
    CHECK(set_panel_collapsed("vp", true).applied == false);
}

TEST_CASE("dock context: set_dock_layout validates and is one undo step")
{
    ContextRig rig;
    DockLayout other = dock_layout();
    REQUIRE(set_panel_open("a", false).applied);
    other = dock_layout();
    rig.frames(2);
    reset_layout();
    CHECK(is_panel_open("a"));
    CHECK(set_dock_layout(other));
    CHECK_FALSE(is_panel_open("a"));
    DockLayout bad;
    REQUIRE(dock_panel(bad, rig.model().panels, pid("a"), k_dock_root, DropZone::Centre).applied);
    CHECK_FALSE(set_dock_layout(bad));
    CHECK_FALSE(is_panel_open("a"));
}

TEST_CASE("dock context: two contexts share one DockModel")
{
    ContextRig rig;
    GuiContext second;
    second.share_dock_model(rig.f.context);
    CHECK(&second.dock_model() == &rig.f.context.dock_model());
    CHECK(second.dock_view().model == &rig.f.context.dock_model());
    {
        ContextScope<GuiContext> scope(second);
        CHECK(is_panel_open("a"));
        REQUIRE(set_panel_open("a", false).applied);
        focus_panel("b");
    }
    CHECK_FALSE(is_panel_open("a"));
    CHECK(is_panel_focused("b"));
    {
        ContextScope<GuiContext> scope(second);
        REQUIRE(register_panel("shared-late"));
    }
    CHECK(find_panel(rig.model().panels, pid("shared-late")) != nullptr);
    rig.frames(2);
    CHECK_FALSE(is_empty(panel_rect("b")));
}

TEST_CASE("dock surfaces: ops address a surface and a layout from a missing surface migrates to surface 0")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(float_panel(layout, panels, pid("c"), Rect{ Vec2f(10.0f, 10.0f), Vec2f(120.0f, 90.0f) }, 1));
    require_applied(dock_panel(layout, panels, pid("d"), dock_root(1), DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("e"), DockTarget{ k_dock_root, 1 }, DropZone::Right));
    CHECK(layout.floats[0].surface == 1);
    REQUIRE(layout.roots[1] != k_no_node);
    CHECK_NOTHROW(validate(layout, panels, ValidateFlags{ true }));

    DockLayout kept = layout;
    migrate_surfaces(kept, panels, 2);
    CHECK(equal(kept, layout));

    migrate_surfaces(layout, panels, 1);
    CHECK(layout.floats[0].surface == 0);
    CHECK(layout.roots[1] == k_no_node);
    CHECK(is_open(layout, pid("d")));
    CHECK(is_open(layout, pid("e")));
    CHECK_NOTHROW(validate(layout, panels, ValidateFlags{ true }));
}

TEST_CASE("DockBuilder: the same Dashboard-shaped layout as the registration hints")
{
    MemoryOnlyLayout memory;
    GuiContext context;
    ContextScope<GuiContext> scope(context);
    register_hinted_panels();
    const DockLayout hinted = build_default_layout(panels());

    DockBuilder b;
    const DockNodeRef main = b.root();
    const DockNodeRef side = b.split(main, DropZone::Right, 360.0f);
    b.dock(main, "viewport/0");
    b.dock(side, "view/first").dock(side, "view/second");
    const DockLayout built = b.build();
    CHECK_MESSAGE(equal(built, hinted), diff(built, hinted));

    REQUIRE(set_default_layout(built));
    static_cast<void>(set_group_open("views", false));
    reset_layout();
    CHECK(equal(dock_layout(), built));
}

TEST_CASE("DockBuilder: the new side keeps its zone, fixed size and collapse flag, and refs survive splitting")
{
    MemoryOnlyLayout memory;
    GuiContext context;
    ContextScope<GuiContext> scope(context);
    register_hinted_panels();
    DockBuilder b;
    const DockNodeRef main = b.root();
    const DockNodeRef left = b.split(main, DropZone::Left, 200.0f);
    const DockNodeRef bottom = b.split(main, DropZone::Bottom);
    b.dock(main, "viewport/0").dock(left, "view/first").dock(bottom, "view/second");
    b.collapse(bottom, true);
    const DockLayout layout = b.build();
    CHECK_NOTHROW(validate(layout, panels(), ValidateFlags{ true }));
    bool fixed_first = false;
    for (uint32_t n = 0; n < layout.node_count; ++n)
        fixed_first = fixed_first || (layout.nodes[n].kind == DockNodeKind::Split && layout.nodes[n].mode == DockSizeMode::FixedFirst && layout.nodes[n].points == doctest::Approx(200.0f));
    CHECK(fixed_first);
    CHECK(is_open(layout, pid("view/second")));
}

TEST_CASE("DockBuilder: mistakes throw an Error naming the cause")
{
    MemoryOnlyLayout memory;
    GuiContext context;
    ContextScope<GuiContext> scope(context);
    register_hinted_panels();
    {
        DockBuilder b;
        b.dock(b.root(), "viewport/0").dock(b.root(), "nope");
        CHECK_THROWS_AS(static_cast<void>(b.build()), Error);
    }
    {
        DockBuilder b;
        b.dock(b.root(), "viewport/0").dock(b.root(), "viewport/0");
        CHECK_THROWS_AS(static_cast<void>(b.build()), Error);
    }
    {
        DockBuilder b;
        const DockNodeRef side = b.split(b.root(), DropZone::Right);
        CHECK_THROWS_AS(b.dock(b.root(), "viewport/0"), Error);
        CHECK_THROWS_AS(static_cast<void>(b.split(side, DropZone::Centre)), Error);
        CHECK_THROWS_AS(b.dock(DockNodeRef{ 99 }, "viewport/0"), Error);
    }
    {
        DockBuilder b;
        b.dock(b.root(), "view/first");
        CHECK_THROWS_AS(static_cast<void>(b.build()), Error);
    }
}

TEST_CASE("GuiDockTheme: derived from every GuiTheme and refreshed by set_theme")
{
    MemoryOnlyLayout memory;
    for (const GuiTheme& theme : { dark_gui_theme(), light_gui_theme(), high_contrast_gui_theme() })
    {
        const GuiDockTheme dock = derive_dock_theme(theme);
        CHECK(dock.preview_border.r == doctest::Approx(theme.panel.accent.r));
        CHECK(dock.preview.a == doctest::Approx(0.28f));
        CHECK(dock.guide.a == doctest::Approx(0.88f));
        CHECK(dock.guide_hover.a == doctest::Approx(0.95f));
        CHECK(dock.refusal.g == doctest::Approx(theme.palette[5].g));
    }
    GuiContext context;
    context.set_theme(light_gui_theme());
    CHECK(context.dock_theme().focus_ring.r == doctest::Approx(light_gui_theme().panel.accent.r));
    GuiDockTheme custom = context.dock_theme();
    custom.focus_ring = Colour{ 1.0f, 0.0f, 1.0f, 1.0f };
    context.set_dock_theme(custom);
    CHECK(context.dock_theme().focus_ring.g == doctest::Approx(0.0f));
    context.set_theme(dark_gui_theme());
    CHECK(context.dock_theme().focus_ring.g == doctest::Approx(dark_gui_theme().panel.accent.g));
}
