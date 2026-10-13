#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

// Nodes of make_sample: 0 split (ratio 0.7), 1 tabs [a b*], 2 tabs [vp*].
struct FloatScene
{
    GuiFixture f;
    DockLayout& layout = f.context.dock_model().layout;
    DockLayout original;

    bool with_menu = false;

    explicit FloatScene(TearOff tear_off = TearOff::Live, TabCycle cycle = TabCycle::Layout)
    {
        set_dock_style([tear_off, cycle](DockStyle& style) { style.tear_off = tear_off; style.tab_cycle = cycle; });
        f.driver.input().surface_size = { 800.0f, 600.0f };
        model().panels = make_panels();
        layout = make_sample(model().panels);
        settle_dock(model());
        original = layout;
        frames(4);
    }

    DockModel& model() { return f.context.dock_model(); }
    const SolvedLayout& solved() { return f.context.dock_view().solved; }

    void frames(uint32_t count = 3)
    {
        auto build = f.frame_of(
            [this]
            {
                if (with_menu)
                {
                    gui::MenuBarScope bar("bar");
                    gui::MenuScope file("File");
                    if (file.open())
                        gui::menu_item("Open");
                }
                PanelHostScope host;
                for (const char* name : { "a", "b", "c", "vp" })
                {
                    PanelScope panel(name);
                    if (panel.visible())
                        gui::label("body");
                }
            });
        f.driver.run_frames(count, build);
    }

    void press_at(const Vec2f& at)
    {
        f.driver.move_to(at);
        frames(1);
        f.driver.press();
        frames(1);
    }

    void move_to(const Vec2f& at)
    {
        const Vec2f from = f.driver.input().pointer.position;
        for (uint32_t step = 1; step <= 6; ++step)
        {
            f.driver.move_to(from + (at - from) * (static_cast<float>(step) / 6.0f));
            frames(1);
        }
    }

    void release() { f.driver.release(); frames(4); }
    void drag(const Vec2f& from, const Vec2f& to) { press_at(from); move_to(to); release(); }
};

Vec2f centre(const Rect& rect) { return Vec2f(rect.min[0] + rect.size[0] * 0.5f, rect.min[1] + rect.size[1] * 0.5f); }

uint32_t float_of(const DockLayout& layout, std::string_view name)
{
    for (uint32_t f = 0; f < layout.float_count; ++f)
        if (layout.floats[f].panel == pid(name))
            return f;
    return k_max_dock_floats;
}

}

TEST_CASE("resize_float moves only the dragged edges and holds the opposite edge at the minimum")
{
    const Rect surface{ { 0.0f, 0.0f }, { 400.0f, 300.0f } };
    const Rect anchor{ { 100.0f, 100.0f }, { 200.0f, 120.0f } };
    const Vec2f least(80.0f, 60.0f);

    const Rect right = resize_float(anchor, k_edge_right, { 30.0f, 99.0f }, least, surface);
    CHECK(right == Rect{ { 100.0f, 100.0f }, { 230.0f, 120.0f } });
    const Rect left = resize_float(anchor, k_edge_left, { -40.0f, 0.0f }, least, surface);
    CHECK(left == Rect{ { 60.0f, 100.0f }, { 240.0f, 120.0f } });
    const Rect corner = resize_float(anchor, k_edge_left | k_edge_top, { 25.0f, -20.0f }, least, surface);
    CHECK(corner == Rect{ { 125.0f, 80.0f }, { 175.0f, 140.0f } });

    const Rect crushed = resize_float(anchor, k_edge_left | k_edge_top, { 500.0f, 500.0f }, least, surface);
    CHECK(crushed == Rect{ { 220.0f, 160.0f }, { 80.0f, 60.0f } });
    const Rect crushed_right = resize_float(anchor, k_edge_right | k_edge_bottom, { -500.0f, -500.0f }, least, surface);
    CHECK(crushed_right == Rect{ { 100.0f, 100.0f }, { 80.0f, 60.0f } });

    const Rect fitted = resize_float(anchor, k_edge_right | k_edge_bottom, { 900.0f, 900.0f }, least, surface);
    CHECK(fitted == Rect{ { 100.0f, 100.0f }, { 300.0f, 200.0f } });
    const Rect off_left = resize_float(anchor, k_edge_left, { -900.0f, 0.0f }, least, surface);
    CHECK(off_left == Rect{ { 0.0f, 100.0f }, { 300.0f, 120.0f } });
}

TEST_CASE("tear_off_due follows ImGui's threshold: vertical distance grows with sideways travel, strip ends tear outward")
{
    FloatScene scene(TearOff::OnRelease);
    const SolvedLayout& solved = scene.solved();
    const float unit = solved.metrics.guide_unit;
    const Rect tab = solved.nodes[1].tab_rects[0];
    const Vec2f inside = centre(tab);

    CHECK_FALSE(tear_off_due(scene.layout, solved, 1, 0, inside, 0.0f));
    CHECK_FALSE(tear_off_due(scene.layout, solved, 1, 0, Vec2f(inside[0], tab.min[1] + tab.size[1] + 1.4f * unit), 0.0f));
    CHECK(tear_off_due(scene.layout, solved, 1, 0, Vec2f(inside[0], tab.min[1] + tab.size[1] + 1.6f * unit), 0.0f));
    CHECK(tear_off_due(scene.layout, solved, 1, 0, Vec2f(inside[0], tab.min[1] - 1.6f * unit), 0.0f));
    CHECK_FALSE(tear_off_due(scene.layout, solved, 1, 0, Vec2f(inside[0], tab.min[1] + tab.size[1] + 1.6f * unit), 12.0f * unit));
    CHECK(tear_off_due(scene.layout, solved, 1, 0, Vec2f(inside[0], tab.min[1] + tab.size[1] + 5.6f * unit), 12.0f * unit));

    CHECK(tear_off_due(scene.layout, solved, 1, 0, Vec2f(tab.min[0] - 2.3f * unit, inside[1]), 0.0f));
    CHECK_FALSE(tear_off_due(scene.layout, solved, 1, 0, Vec2f(tab.min[0] + tab.size[0] + 2.3f * unit, inside[1]), 0.0f));
    const Rect last = solved.nodes[1].tab_rects[1];
    CHECK(tear_off_due(scene.layout, solved, 1, 1, Vec2f(last.min[0] + last.size[0] + 2.3f * unit, inside[1]), 0.0f));
    CHECK_FALSE(tear_off_due(scene.layout, solved, 1, 1, Vec2f(last.min[0] - 2.3f * unit, inside[1]), 0.0f));
}

TEST_CASE("tear_off_rect keeps the grab offset, takes the stack's size and stays on the surface")
{
    FloatScene scene(TearOff::OnRelease);
    const SolvedLayout& solved = scene.solved();
    const Rect node = solved.nodes[1].rect;
    const Vec2f grab(10.0f, 5.0f);
    const Rect rect = tear_off_rect(scene.model().panels, solved, 1, pid("b"), Vec2f(300.0f, 100.0f), grab);
    CHECK(rect.min[0] == doctest::Approx(290.0f));
    CHECK(rect.min[1] == doctest::Approx(95.0f));
    CHECK(rect.size[0] == doctest::Approx(math::min(node.size[0], solved.metrics.float_width * 1.5f)));
    const Rect cornered = tear_off_rect(scene.model().panels, solved, 1, pid("b"), Vec2f(799.0f, 599.0f), grab);
    CHECK(cornered.min[0] + cornered.size[0] <= 800.0f);
    CHECK(cornered.min[1] + cornered.size[1] <= 600.0f);
}

TEST_CASE("tear-off: a held tab becomes a float as soon as it leaves the strip and follows the pointer")
{
    FloatScene scene;
    const Rect tab = scene.solved().nodes[1].tab_rects[1];
    const Vec2f grab_at = centre(tab);
    scene.press_at(grab_at);
    scene.move_to(grab_at + Vec2f(0.0f, 90.0f));

    REQUIRE(scene.layout.float_count == 1);
    CHECK(scene.layout.floats[0].panel == pid("b"));
    CHECK(scene.layout.nodes[1].count == 1);
    CHECK(scene.model().drag.source == DragSource::Float);
    CHECK(is_panel_focused("b"));
    CHECK(is_panel_floating("b"));
    scene.frames(2);
    const Vec2f pointer = scene.f.driver.input().pointer.position;
    CHECK(contains(scene.solved().float_parts[0].title, pointer));
    CHECK(pointer[0] - scene.layout.floats[0].rect.min[0] == doctest::Approx(grab_at[0] - tab.min[0]));

    scene.move_to(pointer + Vec2f(-60.0f, 40.0f));
    CHECK(scene.layout.floats[0].rect.min[0] < tab.min[0]);
    scene.release();
    REQUIRE(scene.layout.float_count == 1);
    CHECK_NOTHROW(validate(scene.layout, scene.model().panels, ValidateFlags{ true }));
}

TEST_CASE("tear-off: reordering along the strip never tears and a torn tab docks back onto a guide")
{
    FloatScene scene;
    const Rect strip = scene.solved().nodes[1].strip;
    const Vec2f from = centre(scene.solved().nodes[1].tab_rects[1]);
    scene.press_at(from);
    scene.move_to(Vec2f(strip.min[0] + 6.0f, from[1] + 4.0f));
    scene.release();
    CHECK(scene.layout.float_count == 0);

    const Rect tab = scene.solved().nodes[1].tab_rects[1];
    scene.press_at(centre(tab));
    scene.move_to(centre(tab) + Vec2f(0.0f, 100.0f));
    REQUIRE(scene.model().drag.source == DragSource::Float);
    const Rect body = scene.solved().nodes[1].body;
    const DropGuides compass = drop_guides(scene.layout, scene.model().panels, scene.solved(), scene.model().drag.panel, centre(body));
    const int32_t right = guide_index(compass, 1, DropZone::Right);
    REQUIRE(right >= 0);
    scene.move_to(centre(compass.guides[right].rect));
    scene.release();
    CHECK(scene.layout.float_count == 0);
    CHECK(is_open(scene.layout, pid("b")));
}

TEST_CASE("tear-off: escape or a refused drop restores the layout from before the tear")
{
    {
        FloatScene scene;
        const Vec2f from = centre(scene.solved().nodes[1].tab_rects[1]);
        scene.press_at(from);
        scene.move_to(from + Vec2f(0.0f, 100.0f));
        REQUIRE(scene.layout.float_count == 1);
        scene.f.driver.key_press(ImKey::Escape);
        scene.frames(1);
        scene.release();
        CHECK(diff(scene.layout, scene.original).empty());
    }
    {
        FloatScene scene;
        never_dock_in("b", "vp");
        const Vec2f from = centre(scene.solved().nodes[1].tab_rects[1]);
        scene.drag(from, centre(scene.solved().nodes[2].strip));
        CHECK(diff(scene.layout, scene.original).empty());
    }
}

TEST_CASE("tear-off: on_release floats only outside the surface, never does not float at all, and no_tear_off pins a panel")
{
    {
        FloatScene scene(TearOff::OnRelease);
        const Vec2f from = centre(scene.solved().nodes[1].tab_rects[1]);
        scene.drag(from, from + Vec2f(0.0f, 150.0f));
        CHECK(scene.layout.float_count == 0);
    }
    {
        FloatScene scene(TearOff::Never);
        const Vec2f from = centre(scene.solved().nodes[1].tab_rects[1]);
        scene.drag(from, from + Vec2f(0.0f, 150.0f));
        CHECK(scene.layout.float_count == 0);
        scene.drag(from, Vec2f(900.0f, 300.0f));
        CHECK(scene.layout.float_count == 0);
        CHECK(diff(scene.layout, scene.original).empty());
    }
    {
        FloatScene scene;
        set_flags(scene.model().panels, "b", panel_flag::all & ~panel_flag::tear_off);
        const Vec2f from = centre(scene.solved().nodes[1].tab_rects[1]);
        scene.drag(from, from + Vec2f(0.0f, 150.0f));
        CHECK(scene.layout.float_count == 0);
    }
}

TEST_CASE("float grips: every edge and corner resizes its side only, and no_resize removes them")
{
    struct Case
    {
        Vec2f at;
        Vec2f push;
        bool left;
        bool top;
        bool right;
        bool bottom;
    };
    FloatScene seed(TearOff::Live);
    const Rect start{ { 200.0f, 150.0f }, { 300.0f, 220.0f } };
    for (uint32_t pick = 0; pick < 8; ++pick)
    {
        FloatScene scene;
        require_applied(float_panel(scene.layout, scene.model().panels, pid("b"), start));
        scene.frames(3);
        const float band = scene.solved().metrics.grip_band;
        const Rect r = scene.layout.floats[0].rect;
        const float x0 = r.min[0];
        const float y0 = r.min[1];
        const float x1 = r.min[0] + r.size[0];
        const float y1 = r.min[1] + r.size[1];
        const float mx = x0 + r.size[0] * 0.5f;
        const float my = y0 + r.size[1] * 0.5f + 20.0f;
        const Vec2f pts[8] = { { x0 + band * 0.2f, y0 - band * 0.5f }, { x1 - band * 0.2f, y0 - band * 0.5f }, { x0 + band * 0.2f, y1 - band * 0.2f }, { x1 - band * 0.2f, y1 - band * 0.2f },
                               { mx, y0 - band * 0.5f }, { mx, y1 + band * 0.2f }, { x0 - band * 0.2f, my }, { x1 + band * 0.2f, my } };
        const bool l = pick == 0 || pick == 2 || pick == 6;
        const bool t = pick == 0 || pick == 1 || pick == 4;
        const bool rr = pick == 1 || pick == 3 || pick == 7;
        const bool b = pick == 2 || pick == 3 || pick == 5;
        scene.drag(pts[pick], pts[pick] + Vec2f(l ? -30.0f : (rr ? 30.0f : 0.0f), t ? -25.0f : (b ? 25.0f : 0.0f)));
        const Rect after = scene.layout.floats[0].rect;
        INFO("grip " << pick);
        CHECK(after.min[0] == doctest::Approx(l ? x0 - 30.0f : x0));
        CHECK(after.min[1] == doctest::Approx(t ? y0 - 25.0f : y0));
        CHECK(after.min[0] + after.size[0] == doctest::Approx(rr ? x1 + 30.0f : x1));
        CHECK(after.min[1] + after.size[1] == doctest::Approx(b ? y1 + 25.0f : y1));
    }
    FloatScene pinned;
    set_flags(pinned.model().panels, "b", panel_flag::all & ~panel_flag::resize);
    require_applied(float_panel(pinned.layout, pinned.model().panels, pid("b"), start));
    pinned.frames(3);
    const Rect r = pinned.layout.floats[0].rect;
    pinned.drag(Vec2f(r.min[0] + r.size[0] + 1.0f, r.min[1] + 100.0f), Vec2f(r.min[0] + r.size[0] + 40.0f, r.min[1] + 100.0f));
    CHECK(pinned.layout.floats[0].rect == r);
}

TEST_CASE("float grips: the stored rect is the shown rect after the window shrinks")
{
    FloatScene scene;
    require_applied(float_panel(scene.layout, scene.model().panels, pid("b"), Rect{ { 500.0f, 400.0f }, { 280.0f, 180.0f } }));
    scene.frames(3);
    scene.f.driver.input().surface_size = { 600.0f, 450.0f };
    scene.frames(4);
    CHECK(scene.layout.floats[0].rect == scene.solved().floats[0]);
    CHECK(scene.layout.floats[0].rect.min[0] + scene.layout.floats[0].rect.size[0] <= 600.0f);
    CHECK(scene.layout.floats[0].rect.min[1] + scene.layout.floats[0].rect.size[1] <= 450.0f);
}

TEST_CASE("ctrl+tab walks nodes depth first and then floats, and starts at the ends from nothing")
{
    FloatScene scene;
    require_applied(float_panel(scene.layout, scene.model().panels, pid("a"), Rect{ { 100.0f, 100.0f }, { 200.0f, 150.0f } }));
    scene.frames(3);
    auto step = [&](bool shift)
    {
        scene.f.driver.input().keys.ctrl = true;
        scene.f.driver.input().keys.shift = shift;
        scene.f.driver.key_press(ImKey::Tab);
        scene.frames(1);
    };
    // Order: b (tabs of node 1), vp (node 2), a (the float).
    focus_panel("b");
    scene.frames(1);
    step(false);
    CHECK(is_panel_focused("vp"));
    step(false);
    CHECK(is_panel_focused("a"));
    step(false);
    CHECK(is_panel_focused("b"));
    step(true);
    CHECK(is_panel_focused("a"));

    scene.model().focused = PanelId{};
    step(false);
    CHECK(is_panel_focused("b"));
    scene.model().focused = PanelId{};
    step(true);
    CHECK(is_panel_focused("a"));
}

TEST_CASE("metrics: dock constants scale with the theme and the float channel sits below popups")
{
    static_assert(k_channel_float < k_channel_popup && k_channel_popup < k_channel_tooltip && k_channel_tooltip < k_channel_drag);
    FloatScene scene;
    const DockMetrics base = scene.solved().metrics;

    GuiTheme big = scene.f.context.gui_theme();
    for (ImStyle* style : { &big.tab, &big.button })
        style->text_height *= 2.0f;
    scene.f.context.set_theme(big);
    scene.frames(3);
    const DockMetrics doubled = scene.solved().metrics;
    CHECK(doubled.close_size == doctest::Approx(base.close_size * 2.0f));
    CHECK(doubled.close_icon == doctest::Approx(base.close_icon * 2.0f));
    CHECK(doubled.grip_band == doctest::Approx(base.grip_band * 2.0f));
    CHECK(doubled.splitter_pad == doctest::Approx(base.splitter_pad * 2.0f));
    CHECK(doubled.float_width == doctest::Approx(base.float_width * 2.0f));
}

namespace
{

bool near_rect(const Rect& a, const Rect& b)
{
    return std::abs(a.min[0] - b.min[0]) < 0.5f && std::abs(a.min[1] - b.min[1]) < 0.5f && std::abs(a.size[0] - b.size[0]) < 0.5f && std::abs(a.size[1] - b.size[1]) < 0.5f;
}

bool same_colour(const Colour& a, const Colour& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

// A node painting nothing but a border in the focus colour around `rect`.
bool has_focus_ring(GuiFixture& f, const Rect& rect)
{
    const LayoutTree& tree = f.context.layout();
    for (uint32_t i = 0; i < tree.node_count(); ++i)
    {
        const LayoutNode& node = tree.node(i);
        if (!node.paint.has_fill && node.paint.border_width > 0.0f && same_colour(node.paint.border, f.context.dock_theme().focus_ring) && near_rect(node.rect, rect))
            return true;
    }
    return false;
}

}

TEST_CASE("focus ring: a focused viewport and a focused collapsed stack are outlined, unfocused ones are not")
{
    FloatScene scene;
    focus_panel("vp");
    scene.frames(3);
    CHECK(has_focus_ring(scene.f, scene.solved().nodes[2].body));
    focus_panel("a");
    scene.frames(3);
    CHECK_FALSE(has_focus_ring(scene.f, scene.solved().nodes[2].body));

    require_applied(set_collapsed(scene.layout, scene.model().panels, 1, true));
    focus_panel("b");
    scene.frames(3);
    REQUIRE(scene.solved().nodes[1].collapsed);
    CHECK(has_focus_ring(scene.f, scene.solved().nodes[1].strip));
    focus_panel("vp");
    scene.frames(3);
    CHECK_FALSE(has_focus_ring(scene.f, scene.solved().nodes[1].strip));
}

TEST_CASE("float layer: floats draw on the float channel and an open menu on the popup channel above it")
{
    FloatScene scene;
    scene.with_menu = true;
    require_applied(float_panel(scene.layout, scene.model().panels, pid("a"), Rect{ { 10.0f, 10.0f }, { 300.0f, 200.0f } }));
    scene.frames(4);
    scene.press_at(rect_centre(scene.f.find_text("File")->rect));
    scene.release();
    scene.frames(2);
    REQUIRE(scene.f.find_text("Open") != nullptr);

    bool float_node = false;
    bool popup_node = false;
    const LayoutTree& tree = scene.f.context.layout();
    for (uint32_t i = 0; i < tree.node_count(); ++i)
    {
        float_node = float_node || tree.node(i).style.channel == k_channel_float;
        popup_node = popup_node || tree.node(i).style.channel == k_channel_popup;
    }
    CHECK(float_node);
    CHECK(popup_node);
}

TEST_CASE("viewport press focuses the viewport and is_panel_floating follows the layout")
{
    FloatScene scene;
    focus_panel("a");
    scene.frames(2);
    CHECK_FALSE(is_panel_floating("b"));
    scene.press_at(rect_centre(scene.solved().nodes[2].body));
    scene.release();
    CHECK(is_panel_focused("vp"));
    require_applied(float_panel(scene.layout, scene.model().panels, pid("b"), Rect{ { 100.0f, 100.0f }, { 240.0f, 180.0f } }));
    scene.frames(2);
    CHECK(is_panel_floating("b"));
    CHECK_FALSE(is_panel_floating("vp"));
    CHECK_FALSE(is_panel_floating("nope"));
}

TEST_CASE("recent ctrl+tab: the first press goes to the previous panel, more presses walk deeper, releasing ctrl commits")
{
    FloatScene scene(TearOff::Live, TabCycle::Recent);
    auto focus = [&](const char* name) { focus_panel(name); scene.frames(2); };
    focus("a");
    focus("vp");
    focus("b");
    auto tab = [&](bool shift)
    {
        scene.f.driver.input().keys.ctrl = true;
        scene.f.driver.input().keys.shift = shift;
        scene.f.driver.key_press(ImKey::Tab);
        scene.frames(1);
    };
    tab(false);
    CHECK(is_panel_focused("vp"));
    tab(false);
    CHECK(is_panel_focused("a"));
    tab(true);
    CHECK(is_panel_focused("vp"));
    scene.f.driver.input().keys.ctrl = false;
    scene.f.driver.input().keys.shift = false;
    scene.frames(2);
    CHECK(scene.model().recent[0] == pid("vp"));
    tab(false);
    CHECK(is_panel_focused("b"));
}

TEST_CASE("recent ctrl+tab: closed panels leave the history and layout order is unchanged by default")
{
    FloatScene scene(TearOff::Live, TabCycle::Recent);
    focus_panel("a");
    scene.frames(2);
    focus_panel("vp");
    scene.frames(2);
    REQUIRE(scene.model().recent_count >= 2);
    require_applied(close_panel(scene.layout, scene.model().panels, pid("a")));
    scene.frames(3);
    for (uint32_t i = 0; i < scene.model().recent_count; ++i)
        CHECK(scene.model().recent[i] != pid("a"));

    FloatScene layout_order;
    focus_panel("b");
    layout_order.frames(2);
    layout_order.f.driver.input().keys.ctrl = true;
    layout_order.f.driver.key_press(ImKey::Tab);
    layout_order.frames(1);
    CHECK(is_panel_focused("vp"));
}

TEST_CASE("remembered float size: a torn tab comes back at the size it last floated, never above 90% of the surface")
{
    FloatScene scene;
    require_applied(float_panel(scene.layout, scene.model().panels, pid("b"), Rect{ { 100.0f, 100.0f }, { 300.0f, 200.0f } }));
    scene.frames(3);
    require_applied(dock_panel(scene.layout, scene.model().panels, pid("b"), 1, DropZone::Centre));
    scene.frames(3);
    const Rect tab = scene.solved().nodes[1].tab_rects[scene.layout.nodes[1].count - 1u];
    scene.press_at(rect_centre(tab));
    scene.move_to(rect_centre(tab) + Vec2f(0.0f, 100.0f));
    scene.frames(2);
    const uint32_t f = float_of(scene.layout, "b");
    REQUIRE(f < k_max_dock_floats);
    CHECK(scene.layout.floats[f].rect.size[0] == doctest::Approx(300.0f));
    CHECK(scene.layout.floats[f].rect.size[1] == doctest::Approx(200.0f));
    scene.release();

    const Vec2f remembered(2000.0f, 2000.0f);
    const Rect capped = tear_off_rect(scene.model().panels, scene.solved(), 1, pid("b"), Vec2f(300.0f, 100.0f), Vec2f(10.0f, 5.0f), remembered);
    CHECK(capped.size[0] <= 800.0f * 0.9f + 0.01f);
    CHECK(capped.size[1] <= 600.0f * 0.9f + 0.01f);
}
